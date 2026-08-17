/*
 * Talkman DMS helper.
 *
 * Original userspace. Speaks QMI Device Management using QCCI already
 * on the phone. Message IDs/TLVs from public libqmi qmi-service-dms.json.
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

#define MSG_DMS_GET_CAPS 0x0020
#define MSG_DMS_GET_MANUF 0x0021
#define MSG_DMS_GET_MODEL 0x0022
#define MSG_DMS_GET_REV 0x0023
#define MSG_DMS_GET_IDS 0x0025
#define MSG_DMS_GET_MODE 0x002D
#define MSG_DMS_SET_MODE 0x002E
#define MSG_DMS_GET_ACT 0x0031
#define MSG_DMS_GET_BANDS 0x0045
#define MSG_DMS_GET_SKU 0x0046
#define MSG_DMS_FCC_AUTH 0x555F

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
    for (i = 0; i < n && i < 64; i++)
        fprintf(stderr, " %02x", b[i]);
    fputc('\n', stderr);
}

static const char *mode_name(unsigned m) {
    switch (m) {
    case 0:
        return "ONLINE";
    case 1:
        return "LPM";
    case 2:
        return "FTM";
    case 3:
        return "OFFLINE";
    case 4:
        return "RESET";
    case 5:
        return "SHUTTING_DOWN";
    case 6:
        return "PERSISTENT_LPM";
    default:
        return "UNKNOWN";
    }
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

static int send_raw(unsigned msg, uint8_t *req, unsigned req_len, uint8_t *resp,
                    unsigned resp_cap, unsigned int *recv_len) {
    qmi_client_error_type e;
    memset(resp, 0, resp_cap);
    *recv_len = 0;
    if (req && req_len)
        hex_dump("req", req, req_len);
    e = g_send_raw(g_clnt, msg, req ? req : (void *)"", req_len, resp, resp_cap,
                   recv_len, 8000);
    fprintf(stderr, "send 0x%x qmi=%d recv=%u\n", msg, e, *recv_len);
    hex_dump("resp", resp, *recv_len);
    return e == QMI_NO_ERR ? 0 : -1;
}

static void dms_get_mode(void) {
    uint8_t resp[128];
    unsigned int n = 0;
    uint16_t result, err;
    unsigned i;
    if (send_raw(MSG_DMS_GET_MODE, NULL, 0, resp, sizeof(resp), &n) != 0)
        die("get-mode send failed");
    parse_result(resp, n, &result, &err);
    printf("get-mode qmi_result=%u qmi_error=%u\n", result, err);
    i = 0;
    while (i + 3 <= n) {
        uint8_t t = resp[i];
        uint16_t tlen = (uint16_t)resp[i + 1] | ((uint16_t)resp[i + 2] << 8);
        if (i + 3 + tlen > n)
            break;
        if (t == 0x01 && tlen >= 1)
            printf("  mode=%u (%s)\n", resp[i + 3], mode_name(resp[i + 3]));
        else if (t == 0x10 && tlen >= 2)
            printf("  offline_reason=0x%02x%02x\n", resp[i + 4], resp[i + 3]);
        else if (t == 0x11 && tlen >= 1)
            printf("  hw_restricted=%u\n", resp[i + 3]);
        i += 3 + tlen;
    }
}

static void dms_set_mode(uint8_t mode) {
    uint8_t req[8];
    uint8_t resp[64];
    unsigned int n = 0;
    uint16_t result, err;
    req[0] = 0x01;
    req[1] = 0x01;
    req[2] = 0x00;
    req[3] = mode;
    if (send_raw(MSG_DMS_SET_MODE, req, 4, resp, sizeof(resp), &n) != 0)
        die("set-mode send failed");
    parse_result(resp, n, &result, &err);
    printf("set-mode %u (%s) qmi_result=%u qmi_error=%u\n", mode,
           mode_name(mode), result, err);
}

static void dms_get_rev(void) {
    uint8_t resp[256];
    unsigned int n = 0;
    uint16_t result, err;
    unsigned i;
    if (send_raw(MSG_DMS_GET_REV, NULL, 0, resp, sizeof(resp), &n) != 0)
        die("get-rev send failed");
    parse_result(resp, n, &result, &err);
    printf("get-rev qmi_result=%u qmi_error=%u\n", result, err);
    i = 0;
    while (i + 3 <= n) {
        uint8_t t = resp[i];
        uint16_t tlen = (uint16_t)resp[i + 1] | ((uint16_t)resp[i + 2] << 8);
        if (i + 3 + tlen > n)
            break;
        if ((t == 0x01 || t == 0x10) && tlen > 0)
            printf("  tlv 0x%02x=%.*s\n", t, (int)tlen, (const char *)(resp + i + 3));
        i += 3 + tlen;
    }
}

static void *load_qmi(const char *name) {
    void *h = dlopen(name, RTLD_NOW | RTLD_GLOBAL);
    if (!h)
        fprintf(stderr, "dlopen %s: %s\n", name, dlerror());
    return h;
}

static void connect_dms(void) {
    void *cci, *svc, *obj = NULL;
    fn_get_svc_obj get_obj;
    int minv, tool, e;

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
    get_obj = (fn_get_svc_obj)dlsym(svc, "dms_get_service_object_internal_v01");
    if (!g_init || !g_send_raw || !g_release)
        die("missing QCCI symbol");
    if (get_obj) {
        for (minv = 0; minv <= 20 && !obj; minv++)
            for (tool = 1; tool <= 12 && !obj; tool++)
                obj = get_obj(1, minv, tool);
    }
    if (!obj)
        obj = dlsym(svc, "dms_qmi_idl_service_object_v01");
    if (!obj)
        die("no DMS service object");
    e = g_init(obj, QMI_CLIENT_INSTANCE_ANY, NULL, NULL, NULL, 4, &g_clnt);
    if (e != QMI_NO_ERR)
        die("qmi_client_init_instance: %d", e);
    fprintf(stderr, "DMS client ok\n");
}

static void print_tlvs(const uint8_t *buf, unsigned n) {
    unsigned i = 0;
    while (i + 3 <= n) {
        uint8_t t = buf[i];
        uint16_t tlen = (uint16_t)buf[i + 1] | ((uint16_t)buf[i + 2] << 8);
        const uint8_t *v = buf + i + 3;
        unsigned k;
        if (i + 3 + tlen > n)
            break;
        printf("  tlv 0x%02x len=%u", t, tlen);
        if (tlen > 0 && tlen <= 64) {
            int ascii = 1;
            for (k = 0; k < tlen; k++) {
                if (v[k] < 32 || v[k] > 126)
                    ascii = 0;
            }
            if (ascii)
                printf(" \"%.*s\"", (int)tlen, (const char *)v);
            else {
                printf(" hex");
                for (k = 0; k < tlen && k < 16; k++)
                    printf(" %02x", v[k]);
            }
        } else if (tlen > 64)
            printf(" (<%u bytes>)", tlen);
        printf("\n");
        i += 3 + tlen;
    }
}

static void dms_simple(unsigned msg, const char *name) {
    uint8_t resp[512];
    unsigned int n = 0;
    uint16_t result, err;
    if (send_raw(msg, NULL, 0, resp, sizeof(resp), &n) != 0)
        die("%s send failed", name);
    parse_result(resp, n, &result, &err);
    printf("%s qmi_result=%u qmi_error=%u recv=%u\n", name, result, err, n);
    print_tlvs(resp, n);
}

static void dms_get_ids(void) {
    uint8_t resp[256];
    unsigned int n = 0;
    uint16_t result, err;
    unsigned i;
    if (send_raw(MSG_DMS_GET_IDS, NULL, 0, resp, sizeof(resp), &n) != 0)
        die("get-ids send failed");
    parse_result(resp, n, &result, &err);
    printf("get-ids qmi_result=%u qmi_error=%u\n", result, err);
    i = 0;
    while (i + 3 <= n) {
        uint8_t t = resp[i];
        uint16_t tlen = (uint16_t)resp[i + 1] | ((uint16_t)resp[i + 2] << 8);
        const uint8_t *v = resp + i + 3;
        if (i + 3 + tlen > n)
            break;
        if (t == 0x11 && tlen > 0)
            printf("  imei_len=%u last2=%.*s\n", tlen,
                   tlen >= 2 ? 2 : (int)tlen, (const char *)(v + (tlen >= 2 ? tlen - 2 : 0)));
        else if (t == 0x10 && tlen > 0)
            printf("  esn_len=%u\n", tlen);
        else if (t == 0x12 && tlen > 0)
            printf("  meid_len=%u\n", tlen);
        else if (t == 0x13 && tlen > 0)
            printf("  imei_sv=%.*s\n", (int)tlen, (const char *)v);
        i += 3 + tlen;
    }
}

static void dms_get_bands(void) {
    uint8_t resp[256];
    unsigned int n = 0;
    uint16_t result, err;
    unsigned i;
    if (send_raw(MSG_DMS_GET_BANDS, NULL, 0, resp, sizeof(resp), &n) != 0)
        die("get-bands send failed");
    parse_result(resp, n, &result, &err);
    printf("get-bands qmi_result=%u qmi_error=%u\n", result, err);
    i = 0;
    while (i + 3 <= n) {
        uint8_t t = resp[i];
        uint16_t tlen = (uint16_t)resp[i + 1] | ((uint16_t)resp[i + 2] << 8);
        const uint8_t *v = resp + i + 3;
        uint64_t x = 0;
        unsigned k;
        if (i + 3 + tlen > n)
            break;
        for (k = 0; k < tlen && k < 8; k++)
            x |= ((uint64_t)v[k]) << (8 * k);
        if (t == 0x01)
            printf("  gsm_wcdma_mask=0x%llx\n", (unsigned long long)x);
        else if (t == 0x10)
            printf("  lte_mask=0x%llx\n", (unsigned long long)x);
        else
            printf("  tlv 0x%02x len=%u val=0x%llx\n", t, tlen,
                   (unsigned long long)x);
        i += 3 + tlen;
    }
}

static void dms_info(void) {
    dms_get_mode();
    dms_get_rev();
    dms_simple(MSG_DMS_GET_MANUF, "get-manuf");
    dms_simple(MSG_DMS_GET_MODEL, "get-model");
    dms_simple(MSG_DMS_GET_CAPS, "get-caps");
    dms_simple(MSG_DMS_GET_ACT, "get-activation");
    dms_simple(MSG_DMS_GET_SKU, "get-sku");
    dms_get_ids();
    dms_get_bands();
}

static void dms_raw(unsigned msg) {
    uint8_t resp[512];
    unsigned int n = 0;
    uint16_t result, err;
    if (send_raw(msg, NULL, 0, resp, sizeof(resp), &n) != 0)
        die("raw send failed");
    parse_result(resp, n, &result, &err);
    printf("raw 0x%x qmi_result=%u qmi_error=%u recv=%u\n", msg, result, err, n);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr,
                "usage: dms-tool info|get-mode|get-rev|get-bands|set-mode <0-6>|fcc|raw <hex>\n");
        return 2;
    }
    setenv("LD_LIBRARY_PATH", "/vendor/lib64:/system/vendor/lib64", 1);
    connect_dms();
    if (strcmp(argv[1], "info") == 0)
        dms_info();
    else if (strcmp(argv[1], "get-mode") == 0)
        dms_get_mode();
    else if (strcmp(argv[1], "get-rev") == 0)
        dms_get_rev();
    else if (strcmp(argv[1], "get-bands") == 0)
        dms_get_bands();
    else if (strcmp(argv[1], "set-mode") == 0 && argc == 3)
        dms_set_mode((uint8_t)atoi(argv[2]));
    else if (strcmp(argv[1], "fcc") == 0)
        dms_simple(MSG_DMS_FCC_AUTH, "fcc-auth");
    else if (strcmp(argv[1], "raw") == 0 && argc == 3)
        dms_raw((unsigned)strtoul(argv[2], NULL, 0));
    else {
        fprintf(stderr,
                "usage: dms-tool info|get-mode|get-rev|get-bands|set-mode <0-6>|fcc|raw <hex>\n");
        return 2;
    }
    if (g_release && g_clnt)
        g_release(g_clnt);
    return 0;
}
