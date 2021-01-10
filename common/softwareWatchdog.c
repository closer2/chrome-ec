/*
 *Copyright 2021 The bitland Authors. All rights reserved.
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
#include "power.h"
#include "power_button.h"
#include "flash.h"


/* Console output macros */
#define CPUTS(outstr) cputs(CC_CHIPSET, outstr)
#define CPRINTS(format, args...) cprints(CC_CHIPSET, format, ##args)

ec_wakeup_WDT g_wakeupWDT = {0};
ec_shutdown_WDT g_shutdownWDT = {0};
static enum ec_status
host_command_WDT(struct host_cmd_handler_args *args)
{
    const struct ec_external_WDT *g_wdtPackage = args->params;

    if (g_wdtPackage == NULL) {
       return EC_RES_INVALID_COMMAND;
    }
    CPRINTS("host_command_WDT: type=%d flag1=%d time=%d\n", g_wdtPackage->type, 
        g_wdtPackage->flag1, g_wdtPackage->time);
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


static void system_sw_wdt_service(void)
{
    /* Poweron software WDT */
    if (g_wakeupWDT.wdtEn == SW_WDT_ENABLE) {
        g_wakeupWDT.countTime++;
        if (g_wakeupWDT.countTime > g_wakeupWDT.time) {
            g_wakeupWDT.countTime = 0;
            g_wakeupWDT.timeoutNum ++;

            //TODO: force shutdown and then power on
            if(POWER_S0 == power_get_state()) {
                CPRINTS("Wakeup WDT timeout(%dsec), force shutdwon", g_wakeupWDT.time);
                chipset_force_shutdown(CHIPSET_WAKEUP_WDT);
            }
        }

        if (g_wakeupWDT.timeoutNum > POWERON_WDT_TIMEOUT_NUM2)
        {
            g_wakeupWDT.wdtEn = SW_WDT_DISENABLE;
        }

        if(POWER_S5 == power_get_state()) {
            CPRINTS("Wakeup WDT timeout, power on times=%d", g_wakeupWDT.timeoutNum);
            power_button_pch_pulse();
            mfg_data_write(MFG_WDT_TIMEOUT_COUNT_OFFSET, g_wakeupWDT.timeoutNum);
        }
    } else {
        g_wakeupWDT.time = 0;
        g_wakeupWDT.countTime = 0;
        g_wakeupWDT.timeoutNum = 0;
    }

    /* shutdown software WDT */
    if (g_shutdownWDT.wdtEn == SW_WDT_ENABLE) {
        g_shutdownWDT.countTime++;
        
        if (g_shutdownWDT.countTime > g_shutdownWDT.time) {
            g_shutdownWDT.countTime = 0;
            g_shutdownWDT.timeoutNum ++;

            //TODO: force shutdown and then power on
            if(POWER_S0 == power_get_state()) {
                CPRINTS("Shutdown WDT timeout(%dsec), force shutdwon",
                            g_shutdownWDT.time);
                /* force shutdwon when beta*/
                chipset_force_shutdown(CHIPSET_WAKEUP_WDT);

                /* notify BIOS NMI when development*/
                /* gpio_set_level(GPIO_APU_NMI_L, 0); */
            }
        }

        if (g_shutdownWDT.timeoutNum > POWERON_WDT_TIMEOUT_NUM)
        {
            g_shutdownWDT.wdtEn = SW_WDT_DISENABLE;
        }
    } else {
        g_shutdownWDT.time = 0;
        g_shutdownWDT.countTime = 0;
        g_shutdownWDT.timeoutNum = 0;

        /* gpio_set_level(GPIO_APU_NMI_L, 1); */
    }
}
DECLARE_HOOK(HOOK_SECOND, system_sw_wdt_service, HOOK_PRIO_INIT_CHIPSET);
 
