/*
 * Talkman UIM helper.
 *
 * Original userspace. Speaks QMI UIM using QCCI already on the phone.
 * Message IDs from public libqmi qmi-service-uim.json.
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
#define MSG_UIM_GET_CARD_STATUS 0x002F
#define MSG_UIM_POWER_OFF 0x0030
#define MSG_UIM_POWER_ON 0x0031
#define MSG_UIM_CHANGE_PROVISIONING 0x0038
/* public libqmi qmi-service-uim.json + public CAF UIM IDL */
#define MSG_UIM_GET_CONFIGURATION 0x003A
#define MSG_UIM_SUBSCRIPTION_OK 0x0040
#define CFG_AUTO_SEL (1u << 0)
#define CFG_PERSO (1u << 1)
#define CFG_HALT_SUBS (1u << 2)
#define CFG_MASK_ALL (CFG_AUTO_SEL | CFG_PERSO | CFG_HALT_SUBS)
#define UIM_SESSION_PRIMARY_GW 0
#define UIM_SLOT_1 1
#define UIM_APP_SIM 1
#define UIM_APP_USIM 2
#define AID_MAX 32

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

static const char *usage =
    "usage: uim-tool card|config|provision-gw|subscription-ok [session]|power-on <slot>|power-off <slot>\n";

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

static void *load_qmi(const char *name) {
    void *h = dlopen(name, RTLD_NOW | RTLD_GLOBAL);
    if (!h)
        fprintf(stderr, "dlopen %s: %s\n", name, dlerror());
    return h;
}

static const char *card_state_name(unsigned s) {
    switch (s) {
    case 0:
        return "ABSENT";
    case 1:
        return "PRESENT";
    case 2:
        return "ERROR";
    default:
        return "?";
    }
}

static const char *app_state_name(unsigned s) {
    switch (s) {
    case 0:
        return "UNKNOWN";
    case 1:
        return "DETECTED";
    case 2:
        return "PIN1_REQUIRED";
    case 3:
        return "PUK1_REQUIRED";
    case 4:
        return "PERSO";
    case 5:
        return "PIN1_BLOCKED";
    case 6:
        return "ILLEGAL";
    case 7:
        return "READY";
    default:
        return "?";
    }
}

static const char *app_type_name(unsigned t) {
    switch (t) {
    case 1:
        return "SIM";
    case 2:
        return "USIM";
    case 3:
        return "RUIM";
    case 4:
        return "CSIM";
    case 5:
        return "ISIM";
    default:
        return "?";
    }
}

static int send_uim_buf(unsigned msg, uint8_t *req, unsigned req_len,
                        uint8_t *resp, unsigned resp_cap, unsigned *out_n,
                        uint16_t *out_result, uint16_t *out_err) {
    unsigned int n = 0;
    unsigned i;
    uint16_t result = 0xffff, err = 0xffff;
    int e;
    e = g_send_raw(g_clnt, msg, req ? req : (void *)"", req_len, resp,
                   resp_cap, &n, 8000);
    hex_dump("resp", resp, n);
    i = 0;
    while (i + 3 <= n) {
        uint8_t t = resp[i];
        uint16_t tlen = (uint16_t)resp[i + 1] | ((uint16_t)resp[i + 2] << 8);
        if (i + 3 + tlen > n)
            break;
        if (t == 0x02 && tlen >= 4) {
            result = (uint16_t)resp[i + 3] | ((uint16_t)resp[i + 4] << 8);
            err = (uint16_t)resp[i + 5] | ((uint16_t)resp[i + 6] << 8);
        }
        i += 3 + tlen;
    }
    printf("msg 0x%x qmi=%d qmi_result=%u qmi_error=%u recv=%u\n", msg, e,
           result, err, n);
    if (out_n)
        *out_n = n;
    if (out_result)
        *out_result = result;
    if (out_err)
        *out_err = err;
    return e == QMI_NO_ERR && result == 0 ? 0 : -1;
}

static int send_uim(unsigned msg, uint8_t *req, unsigned req_len) {
    uint8_t resp[512];
    return send_uim_buf(msg, req, req_len, resp, sizeof(resp), NULL, NULL,
                        NULL);
}

/*
 * Public libqmi "UIM Card Status" (TLV 0x10): 4x uint16 indexes, then cards.
 * App layout matches public CAF user_identity_module_v01 (2016).
 * Prints state only. Does not print IMSI/ICCID.
 */
static int parse_card_status(const uint8_t *buf, unsigned len, uint8_t *aid,
                             unsigned *aid_len) {
    unsigned i = 0;
    const uint8_t *v = NULL;
    uint16_t tlen = 0;
    unsigned off, ncard, c, napp, a;
    uint16_t gw_pri;

    if (aid_len)
        *aid_len = 0;
    while (i + 3 <= len) {
        uint8_t t = buf[i];
        tlen = (uint16_t)buf[i + 1] | ((uint16_t)buf[i + 2] << 8);
        if (i + 3 + tlen > len)
            break;
        if (t == 0x10) {
            v = buf + i + 3;
            break;
        }
        i += 3 + tlen;
    }
    if (!v || tlen < 9)
        return -1;
    gw_pri = (uint16_t)v[0] | ((uint16_t)v[1] << 8);
    printf("index_gw_pri=0x%04x index_1x_pri=0x%04x index_gw_sec=0x%04x "
           "index_1x_sec=0x%04x\n",
           gw_pri, (uint16_t)v[2] | ((uint16_t)v[3] << 8),
           (uint16_t)v[4] | ((uint16_t)v[5] << 8),
           (uint16_t)v[6] | ((uint16_t)v[7] << 8));
    ncard = v[8];
    off = 9;
    for (c = 0; c < ncard && off + 6 <= tlen; c++) {
        unsigned state = v[off];
        unsigned errc = v[off + 4];
        napp = v[off + 5];
        printf("card%u state=%s error=%u apps=%u\n", c, card_state_name(state),
               errc, napp);
        off += 6;
        for (a = 0; a < napp && off + 7 <= tlen; a++) {
            unsigned type = v[off];
            unsigned astate = v[off + 1];
            unsigned alen = v[off + 6];
            unsigned k;
            off += 7;
            if (off + alen + 7 > tlen)
                break;
            printf("  app%u type=%s state=%s aid_len=%u aid=", a,
                   app_type_name(type), app_state_name(astate), alen);
            for (k = 0; k < alen; k++)
                printf("%02x", v[off + k]);
            printf("\n");
            if (aid && aid_len && *aid_len == 0 && alen > 0 && alen <= AID_MAX &&
                (type == UIM_APP_USIM || type == UIM_APP_SIM)) {
                memcpy(aid, v + off, alen);
                *aid_len = alen;
            }
            off += alen + 7; /* univ_pin + pin1 + pin2 */
        }
    }
    return 0;
}

static int send_card(uint8_t *aid, unsigned *aid_len) {
    uint8_t resp[512];
    unsigned n = 0;
    int e;
    e = send_uim_buf(MSG_UIM_GET_CARD_STATUS, NULL, 0, resp, sizeof(resp), &n,
                     NULL, NULL);
    if (e == 0)
        parse_card_status(resp, n, aid, aid_len);
    return e;
}

static void parse_configuration(const uint8_t *buf, unsigned len) {
    unsigned i = 0;
    while (i + 3 <= len) {
        uint8_t t = buf[i];
        uint16_t tlen = (uint16_t)buf[i + 1] | ((uint16_t)buf[i + 2] << 8);
        const uint8_t *v = buf + i + 3;
        if (i + 3 + tlen > len)
            break;
        if (t == 0x10 && tlen >= 1)
            printf("automatic_selection=%u\n", v[0]);
        else if (t == 0x12 && tlen >= 1)
            printf("halt_subscription=%u\n", v[0]);
        else if (t == 0x11 && tlen >= 1) {
            unsigned nfeat = v[0];
            unsigned k;
            printf("personalization_features=%u\n", nfeat);
            for (k = 0; k < nfeat && 1 + (k + 1) * 3 <= tlen; k++)
                printf("  feature=%u verify_left=%u unblock_left=%u\n",
                       v[1 + k * 3], v[2 + k * 3], v[3 + k * 3]);
        } else if (t == 0x13)
            printf("personalization_other_tlv len=%u (not decoded)\n", tlen);
        else if (t != 0x02)
            printf("tlv 0x%02x len=%u\n", t, tlen);
        i += 3 + tlen;
    }
}

static int send_uim_cfg(void) {
    uint8_t req[8];
    uint8_t resp[512];
    unsigned n = 0;
    uint16_t result = 0xffff, err = 0xffff;
    int e;
    req[0] = 0x10;
    req[1] = 0x04;
    req[2] = 0x00;
    req[3] = (uint8_t)CFG_MASK_ALL;
    req[4] = 0;
    req[5] = 0;
    req[6] = 0;
    e = send_uim_buf(MSG_UIM_GET_CONFIGURATION, req, 7, resp, sizeof(resp), &n,
                     &result, &err);
    if (e == 0)
        parse_configuration(resp, n);
    return e;
}

static int send_subscription_ok(unsigned session_type, unsigned ok) {
    uint8_t req[16];
    /*
     * Public CAF user_identity_module_v01: TLV 0x01 session
     * (1-byte type + guint8 AID length, public libqmi "UIM Session"),
     * TLV 0x02 ok_for_subscription (uint8).
     */
    req[0] = 0x01;
    req[1] = 0x02;
    req[2] = 0x00;
    req[3] = (uint8_t)session_type;
    req[4] = 0;
    req[5] = 0x02;
    req[6] = 0x01;
    req[7] = 0x00;
    req[8] = ok ? 1 : 0;
    printf("subscription-ok session=%u ok=%u\n", session_type, ok ? 1 : 0);
    return send_uim(MSG_UIM_SUBSCRIPTION_OK, req, 9);
}

static int send_change_provisioning(unsigned session_type, unsigned slot,
                                    const uint8_t *aid, unsigned aid_len) {
    uint8_t req[64];
    unsigned alen = 2 + 1 + 1 + aid_len;
    unsigned i;
    if (aid_len > AID_MAX)
        return -1;
    /* TLV 0x01 Session Change: type + activate (public libqmi 0x0038) */
    req[0] = 0x01;
    req[1] = 0x02;
    req[2] = 0x00;
    req[3] = (uint8_t)session_type;
    req[4] = 1;
    /* TLV 0x10 Application Information: slot + AID */
    req[5] = 0x10;
    req[6] = (uint8_t)(2 + aid_len);
    req[7] = 0x00;
    req[8] = (uint8_t)slot;
    req[9] = (uint8_t)aid_len;
    memcpy(req + 10, aid, aid_len);
    printf("change-provisioning session=%u activate=1 slot=%u aid_len=%u aid=",
           session_type, slot, aid_len);
    for (i = 0; i < aid_len; i++)
        printf("%02x", aid[i]);
    printf("\n");
    (void)alen;
    return send_uim(MSG_UIM_CHANGE_PROVISIONING, req, 10 + aid_len);
}

static int provision_gw(void) {
    uint8_t aid[AID_MAX];
    unsigned aid_len = 0;
    int e;
    if (send_card(aid, &aid_len) != 0)
        return -1;
    if (aid_len == 0) {
        printf("no SIM/USIM AID on card\n");
        return -1;
    }
    e = send_change_provisioning(UIM_SESSION_PRIMARY_GW, UIM_SLOT_1, aid,
                                 aid_len);
    if (e != 0)
        printf("change-provisioning failed\n");
    /* halt_subscription: first OK can fail until the app leaves DETECTED */
    send_subscription_ok(UIM_SESSION_PRIMARY_GW, 1);
    sleep(3);
    send_subscription_ok(UIM_SESSION_PRIMARY_GW, 1);
    sleep(3);
    return send_card(NULL, NULL);
}

int main(int argc, char **argv) {
    void *cci, *svc, *obj = NULL;
    fn_get_svc_obj get_obj;
    int minv, tool, e;
    uint8_t slot_req[4];

    if (argc < 2) {
        fprintf(stderr, "%s", usage);
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
    get_obj = (fn_get_svc_obj)dlsym(svc, "uim_get_service_object_internal_v01");
    if (!g_init || !g_send_raw || !g_release)
        die("missing QCCI symbol");
    if (get_obj) {
        for (minv = 0; minv <= 20 && !obj; minv++)
            for (tool = 1; tool <= 12 && !obj; tool++)
                obj = get_obj(1, minv, tool);
    }
    if (!obj)
        obj = dlsym(svc, "uim_qmi_idl_service_object_v01");
    if (!obj)
        die("no UIM service object");
    e = g_init(obj, QMI_CLIENT_INSTANCE_ANY, NULL, NULL, NULL, 4, &g_clnt);
    if (e != QMI_NO_ERR)
        die("qmi_client_init_instance: %d", e);
    fprintf(stderr, "UIM client ok\n");
    if (strcmp(argv[1], "card") == 0) {
        send_card(NULL, NULL);
    } else if (strcmp(argv[1], "config") == 0) {
        send_uim_cfg();
    } else if (strcmp(argv[1], "provision-gw") == 0) {
        provision_gw();
    } else if (strcmp(argv[1], "subscription-ok") == 0) {
        unsigned session = UIM_SESSION_PRIMARY_GW;
        if (argc >= 3)
            session = (unsigned)atoi(argv[2]);
        send_subscription_ok(session, 1);
    } else if ((strcmp(argv[1], "power-on") == 0 ||
                strcmp(argv[1], "power-off") == 0) &&
               argc == 3) {
        slot_req[0] = 0x01;
        slot_req[1] = 0x01;
        slot_req[2] = 0x00;
        slot_req[3] = (uint8_t)atoi(argv[2]);
        send_uim(strcmp(argv[1], "power-on") == 0 ? MSG_UIM_POWER_ON
                                                  : MSG_UIM_POWER_OFF,
                 slot_req, 4);
    } else {
        fprintf(stderr, "%s", usage);
        if (g_release && g_clnt)
            g_release(g_clnt);
        return 2;
    }
    if (g_release && g_clnt)
        g_release(g_clnt);
    return 0;
}
