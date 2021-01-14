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

struct chassis_Intrusion {
    uint8_t   chassisIntrusionData;
    uint8_t   chassisIntrusionMode;
    uint8_t   chassisIntrusionModeFlag;
    uint8_t   chassisWriteFlashData;
};

struct chassis_Intrusion  pdata = {
    .chassisIntrusionData = 0, .chassisIntrusionMode = 0,
    .chassisIntrusionModeFlag = 0, .chassisWriteFlashData = 0};

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
                chipset_force_shutdown(LOG_ID_SHUTDOWN_0x09);
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
                chipset_force_shutdown(LOG_ID_SHUTDOWN_0x44);

                /* notify BIOS NMI when development*/
                /* gpio_set_level(GPIO_APU_NMI_L, 0); */
            }
        }

        if (g_shutdownWDT.timeoutNum > POWERON_WDT_TIMEOUT_NUM)
        {
            g_shutdownWDT.wdtEn = SW_WDT_DISENABLE;
        }

        if((POWER_S5==power_get_state()) ||
           (POWER_S3==power_get_state())) {
            g_shutdownWDT.wdtEn = SW_WDT_DISENABLE;
            g_shutdownWDT.timeoutNum = 0;
            g_shutdownWDT.countTime = 0;
            g_shutdownWDT.time = 0;
        }
    } else {
        /* gpio_set_level(GPIO_APU_NMI_L, 1); */
    }
}
DECLARE_HOOK(HOOK_SECOND, system_sw_wdt_service, HOOK_PRIO_INIT_CHIPSET);

uint8_t get_chassisIntrusion_data(void)
{
   return pdata.chassisIntrusionData;
}

void set_chassisIntrusion_data(uint8_t data)
{
    pdata.chassisIntrusionData = data;
}

void set_chassisIntrusion_mode(uint8_t data)
{
    pdata.chassisIntrusionMode = data;
}

void set_chassisIntrusion_mode_flag(uint8_t data)
{
    pdata.chassisIntrusionModeFlag = data;
}

/* set clear crisis circuit host to ec*/
void clear_chassisIntrusion(void)
{
    uint8_t *mptr = host_get_memmap(EC_MEMMAP_POWER_FLAG1);

    /* clear crisis recovery data */
    if (!(*mptr & EC_MEMMAP_CRISIS_CLEAR)) {
        return;
    }

    *mptr &= ~((uint8_t)EC_MEMMAP_CRISIS_CLEAR);

    if (gpio_get_level(GPIO_EC_GPIO0_CASE_OPEN_L)) {

        pdata.chassisIntrusionData = 0x00;
        mfg_data_write(MFG_CHASSIS_INTRUSION_DATA_OFFSET,
            pdata.chassisIntrusionData);

        gpio_set_level(GPIO_EC_CASE_OPEN_CLR, 1);
        msleep(5);
        gpio_set_level(GPIO_EC_CASE_OPEN_CLR, 0);
    }
}

/* set crisis mode  host to ec*/
void set_chassisIntrusion_mode_s(void)
{
    uint8_t *mptr = host_get_memmap(EC_MEMMAP_POWER_FLAG1);

    if (pdata.chassisIntrusionModeFlag != CHASSIS_INTRUSION_MODE_FLAG) {
        return;
    }

    pdata.chassisIntrusionModeFlag = 0x00;

    if (*mptr & EC_MEMMAP_CRISIS_RECOVERY) {
        pdata.chassisIntrusionMode = 0x01;
    } else {
        pdata.chassisIntrusionMode = 0x00;
    }
    mfg_data_write(MFG_CHASSIS_INTRUSION_MODE_OFFSET,
        pdata.chassisIntrusionMode);
}

/*
 * The Fash value of the first boot read is 0xff.
 * For the first time boot pdata->chassisIntrusionMode = 0xff.
 * For the first time boot pdata->chassisIntrusiondata = 0xff.
 * For the first time boot chassisIntrusionMode is open.
 * For the first time boot BIOS notify ec to clear chassis intrusion.
 */
static void Chassis_Intrusion_service(void)
{
    /* set Chassis Intrusion mode */
    set_chassisIntrusion_mode_s();

    /* exit crisis recovery mode */
    if (!pdata.chassisIntrusionMode) {
        return;
    }

    /* enter crisis recovery mode */
    if (pdata.chassisIntrusionData != 0x01) {
        if (gpio_get_level(GPIO_EC_GPIO0_CASE_OPEN_L)) {
            /* get crisis recovery data */
            pdata.chassisIntrusionData = 0x01;
        } else {
            pdata.chassisIntrusionData = 0x00;
        }
        if (pdata.chassisWriteFlashData 
            != pdata.chassisIntrusionData) {
            pdata.chassisWriteFlashData = pdata.chassisIntrusionData;
            mfg_data_write(MFG_CHASSIS_INTRUSION_DATA_OFFSET,
                pdata.chassisIntrusionData);
        }
    }

    /* clear chassis intrusion */
    clear_chassisIntrusion();
}

DECLARE_HOOK(HOOK_MSEC, Chassis_Intrusion_service, HOOK_PRIO_DEFAULT);

#ifdef CONFIG_CONSOLE_CHASSIS_TEST
static int cc_chassisinfo(int argc, char **argv)
{
    char leader[20] = "";
    uint8_t *mptr = host_get_memmap(EC_MEMMAP_POWER_FLAG1);

    ccprintf("%sChassisIntrusionMode: %2d C\n", leader,
            pdata.chassisIntrusionMode);

    ccprintf("%sChassisIntrusionData: %2d C\n", leader,
            pdata.chassisIntrusionData);

    ccprintf("%sGPIO_EC_CASE_OPEN_CLR status: %2d C\n", leader,
        gpio_get_level(GPIO_EC_CASE_OPEN_CLR));

    ccprintf("%sGPIO_EC_GPIO0_CASE_OPEN_L status: %2d C\n", leader,
        gpio_get_level(GPIO_EC_GPIO0_CASE_OPEN_L));

    ccprintf("%sEC_MEMMAP_POWER_FLAG1: %2d C\n", leader, *mptr);
    
    return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(chassisinfo, cc_chassisinfo,
            NULL,
            "Print Sensor info");
#endif

