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
#include "i2c.h"
#include "timer.h"
#include "task.h"
#include "lpc.h"
#include "common.h"
#include "keyboard_protocol.h"


/* Console output macros */
#define CPUTS(outstr) cputs(CC_SWITCH, outstr)
#define CPRINTS(format, args...) cprints(CC_SWITCH, format, ##args)

struct host_byte {
    uint8_t type;
    uint8_t press;
};

/* Private GPIO names are converted to generic GPIO names */

#define TS3A227E_SLAVE_ADDRESS         0x3B /* TS3A227E slave address */

#define TS3A227E_INTERRUPT_FLAG        0x01
#define TS3A227E_KEY_PRESS_INTERRUPT   0x02
#define TS3A227E_INTERRPUT_DISABLE     0x03
#define TS3A227E_DEVICE_SETTING        0x04
#define TS3A227E_DEVICE_SETTING1       0x05
#define TS3A227E_ACCESSORY_STATUS      0x0B

#define TS3A227E_VOLUME_STOP_PRESS       0x01 /* Key 1 press */
#define TS3A227E_VOLUME_STOP_RELEASE     0x02 /* Key 1 relase */
#define TS3A227E_VOLUME_UP_PRESS         0x10 /* Key 3 press */
#define TS3A227E_VOLUME_UP_RELEASE       0x20 /* Key 3 relase */
#define TS3A227E_VOLUME_DOWN_PRESS       0x40 /* Key 4 press */
#define TS3A227E_VOLUME_DOWN_RELEASE     0x80 /* Key 4 relase */

#define TS3A227E_VOLUME_KEY1  (TS3A227E_VOLUME_STOP_PRESS | TS3A227E_VOLUME_STOP_RELEASE)
#define TS3A227E_VOLUME_KEY3  (TS3A227E_VOLUME_UP_PRESS | TS3A227E_VOLUME_UP_RELEASE)
#define TS3A227E_VOLUME_KEY4  (TS3A227E_VOLUME_DOWN_PRESS | TS3A227E_VOLUME_DOWN_RELEASE)

static int TS3A227E_init_config(void)
{
    int rv = 0;
    int regval = 0;

    /* TS3A227E software reset */
    rv = i2c_write8(NPCX_I2C_PORT1_0, TS3A227E_SLAVE_ADDRESS,
        TS3A227E_DEVICE_SETTING, 0xA3);
    if (rv) {
        CPRINTS("TS3A227E software reset error ");
        return rv;
    }

    msleep(20);
    /* TS3A227E interrput enable to INT# pin */
    rv = i2c_write8(NPCX_I2C_PORT1_0, TS3A227E_SLAVE_ADDRESS,
        TS3A227E_INTERRPUT_DISABLE, 0x00);
    if (rv) {
        CPRINTS("TS3A227E write interrput disable error ");
        return rv;
    }

    msleep(2);
    /* TS3A227E Key Press enable */
    rv = i2c_write8(NPCX_I2C_PORT1_0, TS3A227E_SLAVE_ADDRESS,
        TS3A227E_DEVICE_SETTING1, 0x04);
    if (rv) {
        CPRINTS("TS3A227E Key Press enable error ");
        return rv;
    }

    msleep(2);
    /* TS3A227E clear Key Press interrput flag - RC */
    rv = i2c_read8(NPCX_I2C_PORT1_0, TS3A227E_SLAVE_ADDRESS,
        TS3A227E_KEY_PRESS_INTERRUPT, &regval);
    if (rv) {
        CPRINTS("TS3A227E Key Press enable error ");
        return rv;
    }

    msleep(2);
    /* TS3A227E clear interrput flag - RC */
    rv = i2c_read8(NPCX_I2C_PORT1_0, TS3A227E_SLAVE_ADDRESS,
        TS3A227E_INTERRUPT_FLAG, &regval);
    if (rv) {
        CPRINTS("TS3A227E Key Press enable error ");
        return rv;
    }

    CPRINTS("TS3A227E TS3A227E_init_config============== ");

    /* TS3A227E INT# pin enable */
    gpio_enable_interrupt(GPIO_EC_TS3A227_INT);
    return rv;
}

static void TS3A227E_init_config_service(void)
{
    TS3A227E_init_config();
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, TS3A227E_init_config_service, HOOK_PRIO_DEFAULT);

static void TS3A227E_intterrupt_disable(void)
{
    /* TS3A227E INT# pin disable */
    gpio_disable_interrupt(GPIO_EC_TS3A227_INT);
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, TS3A227E_intterrupt_disable, HOOK_PRIO_DEFAULT);

/* TS3A227E INT# pin interrput form gpio.inc */
void audio_ts3a227_interrupt(enum gpio_signal signal)
{
    switch (signal) {
        case GPIO_EC_TS3A227_INT:
            CPRINTS("TS3A227E Front Panel Microphone ising insert");
            task_wake(TASK_ID_TS3A227E);
            break;
        default:
            break;
    }
}

/* TS3A227E handle key press */
static void headset_scancode(int key)
{
    CPRINTS("TS3A227E_KEY_PRESS_INTERRUPT 0x%02x", key);

    if (key & TS3A227E_VOLUME_KEY1) {
        /* TS3A227E handle key press */
       if ((key & TS3A227E_VOLUME_STOP_PRESS) && (key & TS3A227E_VOLUME_STOP_RELEASE)) {
            CPRINTS("volume stop was pressed one time and is not being held.");
        } else if (key & TS3A227E_VOLUME_STOP_PRESS) {
            /* update volume stop key */
            keyboard_update_button(KEYBOARD_BUTTON_VOLUME_PLAY, 1);
        } else if (key & TS3A227E_VOLUME_STOP_RELEASE) {
            /* update volume stop key */
            keyboard_update_button(KEYBOARD_BUTTON_VOLUME_PLAY, 0);
        }
    } else if (key & TS3A227E_VOLUME_KEY3) {
        /* TS3A227E handle key press */
       if ((key & TS3A227E_VOLUME_UP_PRESS) && (key & TS3A227E_VOLUME_UP_RELEASE)) {
            CPRINTS("volume UP was pressed one time and is not being held.");
        } else if (key & TS3A227E_VOLUME_UP_PRESS) {
            /* update volume down key */
            keyboard_update_button(KEYBOARD_BUTTON_VOLUME_UP, 1);
        } else if (key & TS3A227E_VOLUME_UP_RELEASE) {
            /* update volume down key */
            keyboard_update_button(KEYBOARD_BUTTON_VOLUME_UP, 0);
        }
    }  else if (key & TS3A227E_VOLUME_KEY4) {
        /* TS3A227E handle key press */
       if ((key & TS3A227E_VOLUME_DOWN_PRESS) && (key & TS3A227E_VOLUME_DOWN_RELEASE)) {
            CPRINTS("volume DOWN was pressed one time and is not being held.");
        } else if (key & TS3A227E_VOLUME_DOWN_PRESS) {
            /* update volume down key */
            keyboard_update_button(KEYBOARD_BUTTON_VOLUME_DOWN, 1);
        } else if (key & TS3A227E_VOLUME_DOWN_RELEASE) {
            /* update volume down key */
            keyboard_update_button(KEYBOARD_BUTTON_VOLUME_DOWN, 0);
        }
    }
}

void headset_volume_task(void *u)
{
    int rv = 0;
    int regval = 0;

    while (1) {
        int state_timeout = -1;

        rv = i2c_read8(NPCX_I2C_PORT1_0, TS3A227E_SLAVE_ADDRESS,
            TS3A227E_ACCESSORY_STATUS, &regval);
        if (rv) {
            CPRINTS("TS3A227E Read Front Panel Microphone Insertion status error ");
            TS3A227E_init_config();
        }

        /* Check Front Panel Microphone ising insert */
        if (regval & BIT(3)) {
            msleep(2);
            /* TS3A227E clear Key Press interrput flag - RC */
            rv = i2c_read8(NPCX_I2C_PORT1_0, TS3A227E_SLAVE_ADDRESS,
                TS3A227E_KEY_PRESS_INTERRUPT, &regval);
            if (rv) {
                CPRINTS("TS3A227E Key Press enable error ");
                TS3A227E_init_config();
            }
            headset_scancode(regval);
        }

        msleep(2);
        /* TS3A227E clear interrput flag - RC */
        rv = i2c_read8(NPCX_I2C_PORT1_0, TS3A227E_SLAVE_ADDRESS,
            TS3A227E_INTERRUPT_FLAG, &regval);
        if (rv) {
            CPRINTS("TS3A227E Key Press enable error ");
            TS3A227E_init_config();
        }

        msleep(2);
        /* TS3A227E Key Press enable */
        rv = i2c_write8(NPCX_I2C_PORT1_0, TS3A227E_SLAVE_ADDRESS,
            TS3A227E_DEVICE_SETTING1, 0x04);
        if (rv) {
            CPRINTS("TS3A227E Key Press enable error ");
            TS3A227E_init_config();
        }
        task_wait_event(state_timeout);
    }
}

