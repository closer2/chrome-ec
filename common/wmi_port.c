/*
 *Copyright 2021 The bitland Authors. All rights reserved.
 *
 * software watchdog for BLD.
 */
#include "common.h"
#include "console.h"
#include "ec_commands.h"
#include "host_command.h"
#include "util.h"

struct wmi_dfx_ags {
    uint8_t startType;
    uint8_t forntType;
    uint8_t shutdownType;
    uint8_t wakeupType;

    uint16_t forntcode[2];
    uint16_t lastcode[2];
    uint32_t timestamp[2];
};

struct wmi_dfx_ags g_dfxValue = {
    .startType = 0xB0,
    .forntType = 0xB1,
    .shutdownType = 0xE0,
    .wakeupType = 0xE1,
};

#define  wmi_postid(id)             (id == 0 ? 0xFF : id)
#define  wmi_ptimeid(id, time)      (id == 0 ? 0xFFFFFFFF : time)

#define  wmi_scauseid(id)            ((id >> 24) == 0 ? 0xFF : (id >> 24))
#define  wmi_stimesid(id, time)      ((id >> 24) == 0 ? 0xFFFFFFFF : time)
#define  wmi_wcauseid(id)            ((id >> 24) == 0 ? 0xFF : (id >> 24))
#define  wmi_wtimesid(id, time)      ((id >> 24) == 0 ? 0xFFFFFFFF : time)

/* Last POST was booted last time */
void post_last_code_s(void)
{
    g_dfxValue.forntcode[1] = g_dfxValue.forntcode[0];
    g_dfxValue.lastcode[1] = g_dfxValue.lastcode[0];
    // add timestamp
    g_dfxValue.timestamp[1] = g_dfxValue.timestamp[0];
}

void post_last_code(int postcode)
{
    g_dfxValue.forntcode[0] = g_dfxValue.lastcode[0];
    g_dfxValue.lastcode[0] = postcode;
    // add timestamp
    g_dfxValue.timestamp[0] = NPCX_TTC;
}

static enum ec_status
WMI_get_dfx_log(struct host_cmd_handler_args *args)
{
    uint8_t i;
    struct ec_wmi_get_dfx_log *p = args->response;
    uint32_t *smptr = (uint32_t *)host_get_memmap(EC_MEMMAP_SHUTDOWN_CAUSE);
    uint32_t *wmptr = (uint32_t *)host_get_memmap(EC_MEMMAP_WAKEUP_CAUSE);

    if (p == NULL) {
        return EC_RES_INVALID_COMMAND;
    }

    p->startType = (g_dfxValue.startType << 0x08) | 0xFF;       /* 1~2 byte */

    /* postcode, New information comes before old information */
    for (i = 0; i < 2; i++) {
        p->postCode[i].type = (g_dfxValue.forntType << 0x08) | 0xCC;       /* 3~12 byte */
        /* fornt post code */
        p->postCode[i].code0 = wmi_postid(g_dfxValue.lastcode[i]);
        /* last post code */
        p->postCode[i].code1= wmi_postid(g_dfxValue.forntcode[i]);
        /* post code timestamp */
        p->postCode[i].time = 
            wmi_ptimeid(g_dfxValue.lastcode[i], g_dfxValue.timestamp[i]);
    }

    /* shoutdownCase, New information comes before old information */
    for (i = 0; i < 4; i++) {
        p->shutdownCause[i].type = (g_dfxValue.shutdownType << 0x08) | 0xCC;    /* 23~31 byte */
        p->shutdownCause[i].value = wmi_scauseid(*(smptr + i));
        p->shutdownCause[i].reserve = 0xFF;
        p->shutdownCause[i].time = wmi_stimesid(*(smptr + i),*(smptr + i + 1));
    }

    /* wakeupCause, New information comes before old information */
    for (i = 0; i < 4; i++) {
        p->wakeupCause[i].type = (g_dfxValue.wakeupType << 0x08) | 0xCC;       /* 59~67  byte */
        p->wakeupCause[i].value = wmi_wcauseid(*(wmptr + i));
        p->wakeupCause[i].reserve = 0xFFFF;
        p->wakeupCause[i].time = wmi_wtimesid(*(wmptr + i),*(wmptr + i + 1));
    }

    args->response_size = sizeof(*p);
    return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_GET_DFX_LOG,
            WMI_get_dfx_log,
            EC_VER_MASK(0));

/* WMI get dfx log */
static enum ec_status
WMI_get_case_log(struct host_cmd_handler_args *args)
{
    struct ec_wmi_get_cause_log *p = args->response;
    uint32_t *smptr = (uint32_t *)host_get_memmap(EC_MEMMAP_SHUTDOWN_CAUSE);
    uint32_t *wmptr = (uint32_t *)host_get_memmap(EC_MEMMAP_WAKEUP_CAUSE);

    if (p == NULL) {
        return EC_RES_INVALID_COMMAND;
    }

    /* shutdownCause */
    p->shutdownCause.type = (g_dfxValue.shutdownType << 0x08) | 0xCC;       /* 50~58 byte */
    p->shutdownCause.value = wmi_scauseid(*(smptr + 6));
    p->shutdownCause.reserve = 0xFF;
    p->shutdownCause.time = wmi_stimesid(*(smptr + 6),*(smptr + 7));

    /* wakeupCase */
    p->wakeupCause.type = (g_dfxValue.wakeupType << 0x08) | 0xCC;       /* 86~94 byte */
    p->wakeupCause.value = wmi_wcauseid(*(wmptr + 6));
    p->wakeupCause.reserve = 0xFFFF;
    p->wakeupCause.time = wmi_wtimesid(*(wmptr + 6),*(wmptr + 7));

    args->response_size = sizeof(*p);
    return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_GET_CASE_LOG,
            WMI_get_case_log,
            EC_VER_MASK(0));