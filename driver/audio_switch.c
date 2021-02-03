/* Copyright 2021 by Bitland Co.,Ltd. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* for audio switch */


#include "chipset.h"
#include "common.h"
#include "console.h"
#include "gpio.h"
#include "hooks.h"
#include "util.h"
#include "power.h"
#include "timer.h"


/* Console output macros */
#define CPUTS(outstr) cputs(CC_SWITCH, outstr)
#define CPRINTS(format, args...) cprints(CC_SWITCH, format, ##args)

/* Private GPIO names are converted to generic GPIO names */
#define GPIO_FRONT_HP_JD_INPUT  GPIO_EC_FRONT_HP_JD
#define GPIO_REAR_HP_JD_INPUT   GPIO_EC_REAR_HP_JD
#define GPIO_MUX_SWITCH_OUTPUT  GPIO_EC_AUDIO_SWITCH
#define GPIO_JD_OUT_ALC256      GPIO_EC_JD_OUT

static int detect_result = 0;
static int detect_result_backup = 0;

static void headset_detect(void)
{
    if(POWER_S0 != power_get_state()) {
        detect_result = 0;
        detect_result_backup = 0;
        gpio_set_level(GPIO_MUX_SWITCH_OUTPUT, 1);
        gpio_set_level(GPIO_JD_OUT_ALC256, 1);
        
        return;
    }
    
    if(gpio_get_level(GPIO_FRONT_HP_JD_INPUT)) {
        detect_result |= 0x01;
    } else {
        detect_result &= (~0x01);
    }

    if(gpio_get_level(GPIO_REAR_HP_JD_INPUT)) {
        detect_result |= 0x02;
    } else {
        detect_result &= (~0x02);
    }

    if(detect_result != detect_result_backup) {
        detect_result_backup = detect_result;

        gpio_set_level(GPIO_MUX_SWITCH_OUTPUT, (detect_result&0x01)?0:1);
        msleep(10);
        
        gpio_set_level(GPIO_JD_OUT_ALC256, 1);
        msleep(10);
        gpio_set_level(GPIO_JD_OUT_ALC256, 0);

        CPRINTS("Front HP_JD=%d, Rear HP_JD=%d, Switch=%d, detect_result=%X",
            gpio_get_level(GPIO_FRONT_HP_JD_INPUT),
            gpio_get_level(GPIO_REAR_HP_JD_INPUT),
            gpio_get_level(GPIO_MUX_SWITCH_OUTPUT),
            detect_result);
    }
}
DECLARE_HOOK(HOOK_SECOND, headset_detect, HOOK_PRIO_DEFAULT);


