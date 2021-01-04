/*
 * Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * software watchdog for BLD.
 */
 
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "ec_commands.h"
#include "softwareWatchdog.h"
#include <stddef.h>
#include "chipset.h"
#include "system.h"


ec_poweron_WDT g_poweronWDT = {0};

static enum ec_status
host_command_WDT(struct host_cmd_handler_args *args)
{
    const struct ec_external_WDT *g_wdtPackage = args->params;

    if (g_wdtPackage == NULL) {
       return EC_RES_INVALID_COMMAND; 
    }

    switch(g_wdtPackage->flag1) {
        case 1:
            g_poweronWDT.wdtEn = POWERON_WDT_ENABLE;
            break;
        case 2:
            g_poweronWDT.wdtEn = POWERON_WDT_DISENABLE;
            break;
        default:
            break;       
    }
    g_poweronWDT.time = g_wdtPackage->time;
    
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_EXTERNAL_WDT,
		     host_command_WDT,
		     EC_VER_MASK(0)); 

static void SwWatchdogService(void)
{
    if (g_poweronWDT.wdtEn == POWERON_WDT_ENABLE) {
        g_poweronWDT.countTime++;
        if (g_poweronWDT.countTime > g_poweronWDT.time) {
            g_poweronWDT.countTime = 0;
        }

        if (g_poweronWDT.timeoutNum > POWERON_WDT_TIMEOUT_NUM) {
            /* EC reboot */
            system_reset(SYSTEM_RESET_MANUALLY_TRIGGERED);
        }
    } else {
        g_poweronWDT.time = 0;
        g_poweronWDT.countTime = 0;
        g_poweronWDT.timeoutNum = 0;
    }
}
 DECLARE_HOOK(HOOK_SECOND, SwWatchdogService, HOOK_PRIO_INIT_CHIPSET);
 
