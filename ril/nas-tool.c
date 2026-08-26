/*
 * Talkman NAS helper.
 *
 * Original userspace. Speaks QMI Network Access using QCCI already
 * on the phone. Message IDs/TLVs from public libqmi qmi-service-nas.json.
 * System selection preference, RF band info, register, and search.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define QMI_NO_ERR 0
#define QMI_CLIENT_INSTANCE_ANY ((uint32_t)0xffff)

#define MSG_NAS_SET_SSP 0x0033
#define MSG_NAS_GET_RF_BAND 0x0031
#define MSG_NAS_GET_SSP 0x0034
#define MSG_NAS_REGISTER 0x0022
#define MSG_NAS_GET_SERVING 0x0024
#define MSG_NAS_GET_SYS_INFO 0x004D
#define MSG_NAS_FORCE_SEARCH 0x0067
#define MSG_NAS_NETWORK_SCAN 0x0021
#define MSG_NAS_GET_HOME 0x0025
#define MSG_NAS_GET_SIG 0x004F
#define MSG_NAS_BIND_SUBS 0x0045
#define MSG_NAS_ATTACH 0x0023

typedef void *qmi_idl_service_object_type;
typedef void *qmi_client_type;
typedef int qmi_client_error_type;
typedef uint32_t qmi_service_instance;
typedef void (*qmi_client_ind_cb)(qmi_client_type, unsigned int, void *,
                                  unsigned int, void *);
typedef qmi_client_error_type (*fn_init_instance)(
    qmi_idl_service_object_type, qmi_service_instance, qmi_client_ind_cb, void *,
    void *, uint32_t, qmi_client_type *);
typedef qmi_client_error_type (*fn_send_raw)(qmi_client_type, unsigned int,
                                             void *, unsigned int, void *,
                                             unsigned int, unsigned int *,
                                             unsigned int);
typedef qmi_client_error_type (*fn_release)(qmi_client_type);
typedef void *(*fn_get_svc_obj)(int, int, int);

static fn_init_instance g_init;
static fn_send_raw g_send_raw;
static fn_release g_release;
static qmi_client_type g_clnt;

static void die(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

static void hex_dump(const char *tag, const uint8_t *b, unsigned n) {
    unsigned i;
    fprintf(stderr, "%s (%u):", tag, n);
    for (i = 0; i < n && i < 96; i++)
        fprintf(stderr, " %02x", b[i]);
    fputc('\n', stderr);
}

static uint64_t rd_le(const uint8_t *v, unsigned n) {
    uint64_t x = 0;
    unsigned k;
    for (k = 0; k < n && k < 8; k++)
        x |= ((uint64_t)v[k]) << (8 * k);
    return x;
}

static void parse_result(const uint8_t *buf, unsigned len, uint16_t *result,
                         uint16_t *err) {
    unsigned i = 0;
    *result = 0xffff;
    *err = 0xffff;
    while (i + 3 <= len) {
        uint8_t t = buf[i];
        uint16_t tlen = (uint16_t)buf[i + 1] | ((uint16_t)buf[i + 2] << 8);
        if (i + 3 + tlen > len)
            break;
        if (t == 0x02 && tlen >= 4) {
            *result = (uint16_t)buf[i + 3] | ((uint16_t)buf[i + 4] << 8);
            *err = (uint16_t)buf[i + 5] | ((uint16_t)buf[i + 6] << 8);
        }
        i += 3 + tlen;
    }
}

static int send_msg(unsigned msg, const uint8_t *req, unsigned req_len,
                    uint8_t *resp, unsigned resp_cap, unsigned int *recv_len) {
    qmi_client_error_type e;
    memset(resp, 0, resp_cap);
    *recv_len = 0;
    e = g_send_raw(g_clnt, msg, req && req_len ? (void *)req : (void *)"",
                   req_len, resp, resp_cap, recv_len,
                   msg == MSG_NAS_NETWORK_SCAN ? 120000u : 8000u);
    fprintf(stderr, "send 0x%x qmi=%d recv=%u\n", msg, e, *recv_len);
    hex_dump("resp", resp, *recv_len);
    return e == QMI_NO_ERR ? 0 : -1;
}

static int send_raw(unsigned msg, uint8_t *resp, unsigned resp_cap,
                    unsigned int *recv_len) {
    return send_msg(msg, NULL, 0, resp, resp_cap, recv_len);
}

static void nas_set_ssp(uint16_t mode, uint32_t domain) {
    uint8_t req[32];
    uint8_t *p = req;
    uint8_t resp[64];
    unsigned int n = 0;
    uint16_t result, err;
    /* Mode Preference */
    *p++ = 0x11;
    *p++ = 0x02;
    *p++ = 0x00;
    *p++ = (uint8_t)mode;
    *p++ = (uint8_t)(mode >> 8);
    /* Change Duration = immediate (does not persist across reboot) */
    *p++ = 0x17;
    *p++ = 0x01;
    *p++ = 0x00;
    *p++ = 0x01;
    /* Service Domain Preference */
    *p++ = 0x18;
    *p++ = 0x04;
    *p++ = 0x00;
    *p++ = (uint8_t)domain;
    *p++ = (uint8_t)(domain >> 8);
    *p++ = (uint8_t)(domain >> 16);
    *p++ = (uint8_t)(domain >> 24);
    hex_dump("req", req, (unsigned)(p - req));
    if (send_msg(MSG_NAS_SET_SSP, req, (unsigned)(p - req), resp, sizeof(resp),
                 &n) != 0)
        die("set-ssp send failed");
    parse_result(resp, n, &result, &err);
    printf("set-ssp mode=0x%x domain=%u qmi_result=%u qmi_error=%u\n", mode,
           domain, result, err);
}

static void nas_ssp(void) {
    uint8_t resp[512];
    unsigned int n = 0;
    uint16_t result, err;
    unsigned i;
    if (send_raw(MSG_NAS_GET_SSP, resp, sizeof(resp), &n) != 0)
        die("ssp send failed");
    parse_result(resp, n, &result, &err);
    printf("ssp qmi_result=%u qmi_error=%u\n", result, err);
    i = 0;
    while (i + 3 <= n) {
        uint8_t t = resp[i];
        uint16_t tlen = (uint16_t)resp[i + 1] | ((uint16_t)resp[i + 2] << 8);
        const uint8_t *v = resp + i + 3;
        uint64_t x;
        if (i + 3 + tlen > n)
            break;
        x = rd_le(v, tlen);
        switch (t) {
        case 0x10:
            printf("  emergency_mode=%u\n", v[0]);
            break;
        case 0x11:
            printf("  mode_pref=0x%llx\n", (unsigned long long)x);
            break;
        case 0x12:
            printf("  band_pref=0x%llx\n", (unsigned long long)x);
            break;
        case 0x14:
            printf("  roaming_pref=0x%llx\n", (unsigned long long)x);
            break;
        case 0x15:
            printf("  lte_band_pref=0x%llx\n", (unsigned long long)x);
            break;
        case 0x16:
            printf("  net_sel_pref=%u\n", v[0]);
            break;
        case 0x18:
            printf("  srv_domain_pref=0x%llx\n", (unsigned long long)x);
            break;
        default:
            printf("  tlv 0x%02x len=%u val=0x%llx\n", t, tlen,
                   (unsigned long long)x);
            break;
        }
        i += 3 + tlen;
    }
}

static void nas_rfband(void) {
    uint8_t resp[256];
    unsigned int n = 0;
    uint16_t result, err;
    if (send_raw(MSG_NAS_GET_RF_BAND, resp, sizeof(resp), &n) != 0)
        die("rfband send failed");
    parse_result(resp, n, &result, &err);
    printf("rfband qmi_result=%u qmi_error=%u recv=%u\n", result, err, n);
}

static const char *srv_name(unsigned s) {
    switch (s) {
    case 0:
        return "NONE";
    case 1:
        return "LIMITED";
    case 2:
        return "AVAILABLE";
    case 3:
        return "LIMITED_REGIONAL";
    case 4:
        return "POWER_SAVE";
    default:
        return "?";
    }
}

static void nas_sysinfo(void) {
    uint8_t resp[512];
    unsigned int n = 0;
    uint16_t result, err;
    unsigned i;
    if (send_raw(MSG_NAS_GET_SYS_INFO, resp, sizeof(resp), &n) != 0)
        die("sysinfo send failed");
    parse_result(resp, n, &result, &err);
    printf("sysinfo qmi_result=%u qmi_error=%u recv=%u\n", result, err, n);
    i = 0;
    while (i + 3 <= n) {
        uint8_t t = resp[i];
        uint16_t tlen = (uint16_t)resp[i + 1] | ((uint16_t)resp[i + 2] << 8);
        const uint8_t *v = resp + i + 3;
        if (i + 3 + tlen > n)
            break;
        if (t == 0x12 && tlen >= 1)
            printf("  gsm=%s\n", srv_name(v[0]));
        else if (t == 0x13 && tlen >= 1)
            printf("  wcdma=%s\n", srv_name(v[0]));
        else if (t == 0x14 && tlen >= 1)
            printf("  lte=%s\n", srv_name(v[0]));
        else if (t == 0x10 && tlen >= 1)
            printf("  cdma=%s\n", srv_name(v[0]));
        else if (t == 0x27 && tlen >= 4)
            printf("  sim_rej=%u\n", (unsigned)rd_le(v, 4));
        else if (t == 0x2f && tlen >= 4)
            printf("  ns_reg_restriction=%u\n", (unsigned)rd_le(v, 4));
        else if (t != 0x02)
            printf("  tlv 0x%02x len=%u\n", t, tlen);
        i += 3 + tlen;
    }
}

static void nas_serving(void) {
    uint8_t resp[256];
    unsigned int n = 0;
    uint16_t result, err;
    unsigned i;
    if (send_raw(MSG_NAS_GET_SERVING, resp, sizeof(resp), &n) != 0)
        die("serving send failed");
    parse_result(resp, n, &result, &err);
    printf("serving qmi_result=%u qmi_error=%u recv=%u\n", result, err, n);
    i = 0;
    while (i + 3 <= n) {
        uint8_t t = resp[i];
        uint16_t tlen = (uint16_t)resp[i + 1] | ((uint16_t)resp[i + 2] << 8);
        const uint8_t *v = resp + i + 3;
        if (i + 3 + tlen > n)
            break;
        if (t == 0x01 && tlen >= 6)
            printf("  reg=%u cs=%u ps=%u selected=%u radio_ifs=%u first_if=%u\n",
                   v[0], v[1], v[2], v[3], v[4], v[5]);
        else if (t == 0x01 && tlen >= 5)
            printf("  reg=%u cs=%u ps=%u selected=%u radio_ifs=%u\n", v[0],
                   v[1], v[2], v[3], v[4]);
        else if (t != 0x02)
            printf("  tlv 0x%02x len=%u\n", t, tlen);
        i += 3 + tlen;
    }
}

static void nas_register(void) {
    uint8_t req[4];
    uint8_t resp[64];
    unsigned int n = 0;
    uint16_t result, err;
    /* public libqmi Action TLV 0x01: automatic = 1 */
    req[0] = 0x01;
    req[1] = 0x01;
    req[2] = 0x00;
    req[3] = 1;
    if (send_msg(MSG_NAS_REGISTER, req, 4, resp, sizeof(resp), &n) != 0)
        die("register send failed");
    parse_result(resp, n, &result, &err);
    printf("register automatic qmi_result=%u qmi_error=%u\n", result, err);
}

/* CAF nas_v01: TLV 0x16 net_sel_pref = mode(u8) + mcc(u16) + mnc(u16).
 * mode 0 = automatic, 1 = manual. Immediate change duration. */
static void nas_set_plmn(int manual, unsigned mcc, unsigned mnc) {
    uint8_t req[16];
    uint8_t *p = req;
    uint8_t resp[64];
    unsigned int n = 0;
    uint16_t result, err;
    *p++ = 0x16;
    *p++ = 0x05;
    *p++ = 0x00;
    *p++ = manual ? 1 : 0;
    *p++ = (uint8_t)mcc;
    *p++ = (uint8_t)(mcc >> 8);
    *p++ = (uint8_t)mnc;
    *p++ = (uint8_t)(mnc >> 8);
    *p++ = 0x17;
    *p++ = 0x01;
    *p++ = 0x00;
    *p++ = 0x01;
    hex_dump("req", req, (unsigned)(p - req));
    if (send_msg(MSG_NAS_SET_SSP, req, (unsigned)(p - req), resp, sizeof(resp),
                 &n) != 0)
        die("set-plmn send failed");
    parse_result(resp, n, &result, &err);
    printf("set-plmn %s mcc=%u mnc=%u qmi_result=%u qmi_error=%u\n",
           manual ? "manual" : "auto", mcc, mnc, result, err);
}

/* CAF nas_manual_network_register_info TLV 0x10: mcc + mnc + radio_if.
 * radio_if: 4=GSM 5=UMTS 8=LTE (public nas_radio_if_enum_v01). */
static void nas_register_manual(unsigned mcc, unsigned mnc, unsigned rat) {
    uint8_t req[16];
    uint8_t resp[64];
    unsigned int n = 0;
    uint16_t result, err;
    req[0] = 0x01;
    req[1] = 0x01;
    req[2] = 0x00;
    req[3] = 2;
    req[4] = 0x10;
    req[5] = 0x05;
    req[6] = 0x00;
    req[7] = (uint8_t)mcc;
    req[8] = (uint8_t)(mcc >> 8);
    req[9] = (uint8_t)mnc;
    req[10] = (uint8_t)(mnc >> 8);
    req[11] = (uint8_t)rat;
    hex_dump("req", req, 12);
    if (send_msg(MSG_NAS_REGISTER, req, 12, resp, sizeof(resp), &n) != 0)
        die("register-manual send failed");
    parse_result(resp, n, &result, &err);
    printf("register-manual mcc=%u mnc=%u rat=%u qmi_result=%u qmi_error=%u\n",
           mcc, mnc, rat, result, err);
}

static int parse_hex_bytes(int argc, char **argv, int start, uint8_t *out,
                           unsigned cap, unsigned *out_len) {
    int i;
    unsigned n = 0;
    for (i = start; i < argc; i++) {
        unsigned v;
        if (n >= cap)
            return -1;
        v = (unsigned)strtoul(argv[i], NULL, 16);
        if (v > 0xff)
            return -1;
        out[n++] = (uint8_t)v;
    }
    *out_len = n;
    return 0;
}

static void nas_search(void) {
    uint8_t resp[64];
    unsigned int n = 0;
    uint16_t result, err;
    if (send_raw(MSG_NAS_FORCE_SEARCH, resp, sizeof(resp), &n) != 0)
        die("search send failed");
    parse_result(resp, n, &result, &err);
    printf("force-search qmi_result=%u qmi_error=%u\n", result, err);
}

static void nas_home(void) {
    uint8_t resp[256];
    unsigned int n = 0;
    uint16_t result, err;
    unsigned i;
    if (send_raw(MSG_NAS_GET_HOME, resp, sizeof(resp), &n) != 0)
        die("home send failed");
    parse_result(resp, n, &result, &err);
    printf("home qmi_result=%u qmi_error=%u recv=%u\n", result, err, n);
    i = 0;
    while (i + 3 <= n) {
        uint8_t t = resp[i];
        uint16_t tlen = (uint16_t)resp[i + 1] | ((uint16_t)resp[i + 2] << 8);
        const uint8_t *v = resp + i + 3;
        if (i + 3 + tlen > n)
            break;
        if (t == 0x01 && tlen >= 4)
            printf("  mcc=%u mnc=%u\n", (unsigned)rd_le(v, 2),
                   (unsigned)rd_le(v + 2, 2));
        else if (t != 0x02)
            printf("  tlv 0x%02x len=%u\n", t, tlen);
        i += 3 + tlen;
    }
}

static void nas_bind(unsigned subs) {
    uint8_t req[8];
    uint8_t resp[64];
    unsigned int n = 0;
    uint16_t result, err;
    /* public CAF nas_bind_subscription_req: TLV 0x01 subscription type */
    req[0] = 0x01;
    req[1] = 0x01;
    req[2] = 0x00;
    req[3] = (uint8_t)subs;
    hex_dump("req", req, 4);
    if (send_msg(MSG_NAS_BIND_SUBS, req, 4, resp, sizeof(resp), &n) != 0)
        die("bind send failed");
    parse_result(resp, n, &result, &err);
    printf("bind subs=%u qmi_result=%u qmi_error=%u\n", subs, result, err);
}

static void nas_attach(unsigned action) {
    uint8_t req[4];
    uint8_t resp[64];
    unsigned int n = 0;
    uint16_t result, err;
    /* public libqmi Action TLV 0x10: 0 = PS detach, 1 = PS attach */
    req[0] = 0x10;
    req[1] = 0x01;
    req[2] = 0x00;
    req[3] = action ? 1 : 0;
    hex_dump("req", req, 4);
    if (send_msg(MSG_NAS_ATTACH, req, 4, resp, sizeof(resp), &n) != 0)
        die("attach send failed");
    parse_result(resp, n, &result, &err);
    printf("attach action=%u qmi_result=%u qmi_error=%u\n", action, result, err);
}

static void nas_siginfo(void) {
    uint8_t resp[256];
    unsigned int n = 0;
    uint16_t result, err;
    unsigned i;
    if (send_raw(MSG_NAS_GET_SIG, resp, sizeof(resp), &n) != 0)
        die("siginfo send failed");
    parse_result(resp, n, &result, &err);
    printf("siginfo qmi_result=%u qmi_error=%u recv=%u\n", result, err, n);
    i = 0;
    while (i + 3 <= n) {
        uint8_t t = resp[i];
        uint16_t tlen = (uint16_t)resp[i + 1] | ((uint16_t)resp[i + 2] << 8);
        const uint8_t *v = resp + i + 3;
        if (i + 3 + tlen > n)
            break;
        if (t == 0x12 && tlen >= 1)
            printf("  gsm_rssi=%d\n", (int8_t)v[0]);
        else if (t == 0x13 && tlen >= 1)
            printf("  wcdma_rssi=%d\n", (int8_t)v[0]);
        else if (t == 0x14 && tlen >= 1)
            printf("  lte_rssi=%d\n", (int8_t)v[0]);
        else if (t != 0x02)
            printf("  tlv 0x%02x len=%u\n", t, tlen);
        i += 3 + tlen;
    }
}

static void nas_scan(void) {
    uint8_t resp[1024];
    unsigned int n = 0;
    uint16_t result, err;
    unsigned i;
    if (send_raw(MSG_NAS_NETWORK_SCAN, resp, sizeof(resp), &n) != 0)
        die("scan send failed");
    parse_result(resp, n, &result, &err);
    printf("scan qmi_result=%u qmi_error=%u recv=%u\n", result, err, n);
    i = 0;
    while (i + 3 <= n) {
        uint8_t t = resp[i];
        uint16_t tlen = (uint16_t)resp[i + 1] | ((uint16_t)resp[i + 2] << 8);
        if (i + 3 + tlen > n)
            break;
        if (t != 0x02)
            printf("  tlv 0x%02x len=%u\n", t, tlen);
        i += 3 + tlen;
    }
}

static void *load_qmi(const char *name) {
    void *h = dlopen(name, RTLD_NOW | RTLD_GLOBAL);
    if (!h)
        fprintf(stderr, "dlopen %s: %s\n", name, dlerror());
    return h;
}

int main(int argc, char **argv) {
    void *cci, *svc, *obj = NULL;
    fn_get_svc_obj get_obj;
    int minv, tool, e;

    if (argc < 2) {
        fprintf(stderr,
                "usage: nas-tool ssp|rfband|sysinfo|serving|home|siginfo|bind|attach|register|search|raw <hex>|set-ssp <modehex> [domain]|set-plmn auto|manual <mcc> <mnc>|register-manual <mcc> <mnc> [rat]|send <hexmsg> [hex bytes...]\n");
    return 2;
    }
    setenv("LD_LIBRARY_PATH", "/vendor/lib64:/system/vendor/lib64", 1);
    (void)load_qmi("libqmi_common_so.so");
    (void)load_qmi("libqmi_encdec.so");
    (void)load_qmi("libqmi_client_qmux.so");
    cci = load_qmi("libqmi_cci.so");
    svc = load_qmi("libqmiservices.so");
    if (!cci || !svc)
        die("dlopen QCCI");
    g_init = (fn_init_instance)dlsym(cci, "qmi_client_init_instance");
    g_send_raw = (fn_send_raw)dlsym(cci, "qmi_client_send_raw_msg_sync");
    g_release = (fn_release)dlsym(cci, "qmi_client_release");
    get_obj = (fn_get_svc_obj)dlsym(svc, "nas_get_service_object_internal_v01");
    if (!g_init || !g_send_raw || !g_release)
        die("missing QCCI symbol");
    if (get_obj) {
        for (minv = 0; minv <= 20 && !obj; minv++)
            for (tool = 1; tool <= 12 && !obj; tool++)
                obj = get_obj(1, minv, tool);
    }
    if (!obj)
        obj = dlsym(svc, "nas_qmi_idl_service_object_v01");
    if (!obj)
        die("no NAS service object");
    e = g_init(obj, QMI_CLIENT_INSTANCE_ANY, NULL, NULL, NULL, 4, &g_clnt);
    if (e != QMI_NO_ERR)
        die("qmi_client_init_instance: %d", e);
    fprintf(stderr, "NAS client ok\n");
    if (strcmp(argv[1], "ssp") == 0)
        nas_ssp();
    else if (strcmp(argv[1], "rfband") == 0)
        nas_rfband();
    else if (strcmp(argv[1], "sysinfo") == 0)
        nas_sysinfo();
    else if (strcmp(argv[1], "serving") == 0)
        nas_serving();
    else if (strcmp(argv[1], "register") == 0)
        nas_register();
    else if (strcmp(argv[1], "search") == 0)
        nas_search();
    else if (strcmp(argv[1], "home") == 0)
        nas_home();
    else if (strcmp(argv[1], "siginfo") == 0)
        nas_siginfo();
    else if (strcmp(argv[1], "bind") == 0)
        nas_bind(argc >= 3 ? (unsigned)strtoul(argv[2], NULL, 0) : 0u);
    else if (strcmp(argv[1], "attach") == 0)
        nas_attach(argc >= 3 ? (unsigned)strtoul(argv[2], NULL, 0) : 1u);
    else if (strcmp(argv[1], "scan") == 0)
        nas_scan();
    else if (strcmp(argv[1], "raw") == 0 && argc >= 3) {
        uint8_t resp[512];
        unsigned int n = 0;
        uint16_t result, err;
        unsigned msg = (unsigned)strtoul(argv[2], NULL, 0);
        if (send_raw(msg, resp, sizeof(resp), &n) != 0)
            die("raw send failed");
        parse_result(resp, n, &result, &err);
        printf("raw 0x%x qmi_result=%u qmi_error=%u recv=%u\n", msg, result,
               err, n);
    } else if (strcmp(argv[1], "set-ssp") == 0 && argc >= 3)
        nas_set_ssp((uint16_t)strtoul(argv[2], NULL, 0),
                    argc >= 4 ? (uint32_t)strtoul(argv[3], NULL, 0) : 2u);
    else if (strcmp(argv[1], "set-plmn") == 0 && argc >= 3) {
        int manual = (strcmp(argv[2], "manual") == 0);
        unsigned mcc = argc >= 4 ? (unsigned)strtoul(argv[3], NULL, 0) : 0;
        unsigned mnc = argc >= 5 ? (unsigned)strtoul(argv[4], NULL, 0) : 0;
        if (!manual && strcmp(argv[2], "auto") != 0)
            die("set-plmn auto|manual");
        nas_set_plmn(manual, mcc, mnc);
    } else if (strcmp(argv[1], "register-manual") == 0 && argc >= 4)
        nas_register_manual((unsigned)strtoul(argv[2], NULL, 0),
                            (unsigned)strtoul(argv[3], NULL, 0),
                            argc >= 5 ? (unsigned)strtoul(argv[4], NULL, 0)
                                      : 4u);
    else if (strcmp(argv[1], "send") == 0 && argc >= 3) {
        uint8_t req[256];
        uint8_t resp[512];
        unsigned int n = 0;
        unsigned req_len = 0;
        uint16_t result, err;
        unsigned msg = (unsigned)strtoul(argv[2], NULL, 0);
        if (parse_hex_bytes(argc, argv, 3, req, sizeof(req), &req_len) != 0)
            die("send hex parse");
        if (send_msg(msg, req, req_len, resp, sizeof(resp), &n) != 0)
            die("send failed");
        parse_result(resp, n, &result, &err);
        printf("send 0x%x qmi_result=%u qmi_error=%u recv=%u\n", msg, result,
               err, n);
    } else {
        fprintf(stderr,
                "usage: nas-tool ssp|rfband|sysinfo|serving|home|siginfo|bind|attach|register|search|raw <hex>|set-ssp <modehex> [domain]|set-plmn auto|manual <mcc> <mnc>|register-manual <mcc> <mnc> [rat]|send <hexmsg> [hex bytes...]\n");
        if (g_release && g_clnt)
            g_release(g_clnt);
        return 2;
    }
    if (g_release && g_clnt)
        g_release(g_clnt);
    return 0;
}
