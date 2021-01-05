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


ec_wakeup_WDT g_wakeupWDT = {0};
ec_shutdown_WDT g_shutdownWDT = {0};
static enum ec_status
host_command_WDT(struct host_cmd_handler_args *args)
{
    const struct ec_external_WDT *g_wdtPackage = args->params;

    if (g_wdtPackage == NULL) {
       return EC_RES_INVALID_COMMAND; 
    }

    switch(g_wdtPackage->type) {
        case 1:
            if (g_wdtPackage->flag1 == 0x01) {
                g_wakeupWDT.wdtEn = SW_WDT_ENABLE;
            } else if (g_wdtPackage->flag1 == 0x02) {
                g_wakeupWDT.wdtEn = SW_WDT_DISENABLE;
            }
            g_wakeupWDT.time = g_wdtPackage->time;
            break;
        case 2:
            if (g_wdtPackage->flag1 == 0x01) {
                g_shutdownWDT.wdtEn = SW_WDT_ENABLE;
            } else if (g_wdtPackage->flag1 == 0x02) {
                g_shutdownWDT.wdtEn = SW_WDT_DISENABLE;
            }
            g_shutdownWDT.time = g_wdtPackage->time;
            break;
        default:
            break;        
    }
    
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_EXTERNAL_WDT,
		     host_command_WDT,
		     EC_VER_MASK(0)); 

static void SwWatchdogService(void)
{
    /* Poweron software WDT */
    if (g_wakeupWDT.wdtEn == SW_WDT_ENABLE) {
        g_wakeupWDT.countTime++;
        if (g_wakeupWDT.countTime > g_wakeupWDT.time) {
            g_wakeupWDT.countTime = 0;
        }

        if (g_wakeupWDT.timeoutNum > POWERON_WDT_TIMEOUT_NUM) {
            /* EC reboot */
            system_reset(SYSTEM_RESET_MANUALLY_TRIGGERED);
        }
    } else {
        g_wakeupWDT.time = 0;
        g_wakeupWDT.countTime = 0;
        g_wakeupWDT.timeoutNum = 0;
    }
    /* Shutdown software WDT */ 
    if (g_shutdownWDT.wdtEn == SW_WDT_ENABLE) {
        g_shutdownWDT.countTime++;
        if (g_shutdownWDT.countTime > g_shutdownWDT.time) {
            gpio_set_level(GPIO_APU_NMI_L, 0);
        }  
    } else {
        g_shutdownWDT.time = 0;
        g_shutdownWDT.countTime = 0;
        g_shutdownWDT.timeoutNum = 0;
        gpio_set_level(GPIO_APU_NMI_L, 1);
    }
}
 DECLARE_HOOK(HOOK_SECOND, SwWatchdogService, HOOK_PRIO_INIT_CHIPSET);
 
