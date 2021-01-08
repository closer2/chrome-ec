/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "adc.h"
#include "adc_chip.h"
#include "button.h"
#include "chipset.h"
#include "common.h"
#include "cros_board_info.h"
#include "driver/ppc/aoz1380.h"
#include "driver/ppc/nx20p348x.h"
#include "driver/retimer/pi3hdx1204.h"
#include "driver/tcpm/rt1715.h"
#include "extpower.h"
#include "fan.h"
#include "fan_chip.h"
#include "gpio.h"
#include "hooks.h"
#include "power.h"
#include "power/renoir.h"
#include "power_button.h"
#include "pwm.h"
#include "pwm_chip.h"
#include "sb_tsi.h"
#include "switch.h"
#include "system.h"
#include "task.h"
#include "temp_sensor.h"
#include "thermistor.h"
#include "usb_pd_tcpm.h"
#include "usbc_ppc.h"
#include "flash.h"


#define CPRINTSUSB(format, args...) cprints(CC_USBCHARGE, format, ## args)
#define CPRINTFUSB(format, args...) cprintf(CC_USBCHARGE, format, ## args)

#include "gpio_list.h"

/* TODO: confirm with real hardware */
const enum gpio_signal hibernate_wake_pins[] = {
	GPIO_POWER_BUTTON_L,
};
const int hibernate_wake_pins_used =  ARRAY_SIZE(hibernate_wake_pins);

/* TODO: need confirm with real hardware */
const struct power_signal_info power_signal_list[] = {
	[SYSTEM_ALW_PG] = {
		.gpio = GPIO_SYSTEM_ALW_PG,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "SYSTEM_ALW_PG",
	},
	[X86_SLP_S3_N] = {
		.gpio = GPIO_PCH_SLP_S3_L,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "SLP_S3_DEASSERTED",
	},
	[X86_SLP_S5_N] = {
		.gpio = GPIO_PCH_SLP_S5_L,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "SLP_S5_DEASSERTED",
	},
	[ATX_PG] = {
		.gpio = GPIO_ATX_PG,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "ATX_PG",
	},
	[VCORE_EN] = {
		.gpio = GPIO_VCORE_EN,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "VCORE_EN",
	},
	[VRMPWRGD] = {
		.gpio = GPIO_VRMPWRGD,
		.flags = POWER_SIGNAL_ACTIVE_HIGH,
		.name = "VRMPWRGD",
	},
};
BUILD_ASSERT(ARRAY_SIZE(power_signal_list) == POWER_SIGNAL_COUNT);

int board_get_temp(int idx, int *temp_k)
{
	int mv;
	int temp_c;
	enum adc_channel channel;

	/* idx is the sensor index set in board temp_sensors[] */
	switch (idx) {
    case TEMP_SENSOR_SOC_CORE:
		/* thermistor is not powered in G3 */
		if (chipset_in_state(CHIPSET_STATE_HARD_OFF))
			return EC_ERROR_NOT_POWERED;

		channel = ADC_SENSOR_SOC_NEAR;
		break;    
    case TEMP_SENSOR_SSD_NEAR:
		channel = ADC_SENSOR_SSD_NEAR;
		break;
    case TEMP_SENSOR_PCIEX16_NEAR:
		channel = ADC_SENSOR_PCIEX16_NEAR;
		break;
    case TEMP_SENSOR_ENVIRONMENT:
		channel = ADC_SENSOR_ENVIRONMENT;
		break;
    case TEMP_SENSOR_MEMORY_NEAR:
		channel = ADC_SENSOR_MEMORY_NEAR;
		break;  
	default:
		return EC_ERROR_INVAL;
	}

	mv = adc_read_channel(channel);
	if (mv < 0)
		return EC_ERROR_INVAL;

	temp_c = thermistor_linear_interpolate(mv, &thermistor_info);
	*temp_k = C_TO_K(temp_c);
	return EC_SUCCESS;
}

const struct adc_t adc_channels[] = {
	[ADC_SENSOR_SOC_NEAR] = {
		.name = "SOC Near",
		.input_ch = NPCX_ADC_CH0,
		.factor_mul = ADC_MAX_VOLT,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
	},
    [ADC_SENSOR_SSD_NEAR] = {
		.name = "SSD Near",
		.input_ch = NPCX_ADC_CH6,
		.factor_mul = ADC_MAX_VOLT,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
	},
	[ADC_SENSOR_PCIEX16_NEAR] = {
		.name = "PCIE16 Near",
		.input_ch = NPCX_ADC_CH1,
		.factor_mul = ADC_MAX_VOLT,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
	},
    [ADC_SENSOR_ENVIRONMENT] = {
		.name = "Environment Near",
		.input_ch = NPCX_ADC_CH7,
		.factor_mul = ADC_MAX_VOLT,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
	},
    [ADC_SENSOR_MEMORY_NEAR] = {
		.name = "Memory Near",
		.input_ch = NPCX_ADC_CH1,
		.factor_mul = ADC_MAX_VOLT,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
	},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

const struct temp_sensor_t temp_sensors[] = {
	[TEMP_SENSOR_SOC_CORE] = {
		.name = "CPU Core",
		.type = TEMP_SENSOR_TYPE_CPU,
		.read = sb_tsi_get_val,
		.idx = TEMP_SENSOR_SOC_CORE,
	},	
	[TEMP_SENSOR_SOC_NEAR] = {
		.name = "SOC Near",
		.type = TEMP_SENSOR_TYPE_BOARD,
		.read = board_get_temp,
		.idx = TEMP_SENSOR_SOC_NEAR,
	},
    [TEMP_SENSOR_SSD_NEAR] = {
        .name = "SSD Near",
        .type = TEMP_SENSOR_TYPE_BOARD,
        .read = board_get_temp,
        .idx = TEMP_SENSOR_SSD_NEAR,
    },
    [TEMP_SENSOR_PCIEX16_NEAR] = {
        .name = "PCIE16 Near",
        .type = TEMP_SENSOR_TYPE_BOARD,
        .read = board_get_temp,
        .idx = TEMP_SENSOR_PCIEX16_NEAR,
    },
    [TEMP_SENSOR_ENVIRONMENT] = {
        .name = "Environment Near",
        .type = TEMP_SENSOR_TYPE_BOARD,
        .read = board_get_temp,
        .idx = TEMP_SENSOR_ENVIRONMENT,
    },
    [TEMP_SENSOR_MEMORY_NEAR] = {
        .name = "Memory Near",
        .type = TEMP_SENSOR_TYPE_BOARD,
        .read = board_get_temp,
        .idx = TEMP_SENSOR_MEMORY_NEAR,
    },
};
BUILD_ASSERT(ARRAY_SIZE(temp_sensors) == TEMP_SENSOR_COUNT);

__overridable struct ec_thermal_config thermal_params[TEMP_SENSOR_COUNT] = {
	[TEMP_SENSOR_SOC_CORE] = {
		.temp_host = {
			[EC_TEMP_THRESH_HIGH] = C_TO_K(90),
			[EC_TEMP_THRESH_HALT] = C_TO_K(92),
		},
		.temp_host_release = {
			[EC_TEMP_THRESH_HIGH] = C_TO_K(80),
		},
        .temp_fan_off = C_TO_K(25),
	    .temp_fan_max = C_TO_K(45)
	},
    [TEMP_SENSOR_SOC_NEAR] = {
		.temp_host = {
			[EC_TEMP_THRESH_HIGH] = C_TO_K(90),
			[EC_TEMP_THRESH_HALT] = C_TO_K(92),
		},
		.temp_host_release = {
			[EC_TEMP_THRESH_HIGH] = C_TO_K(80),
		},
        .temp_fan_off = C_TO_K(10),
	    .temp_fan_max = C_TO_K(40)
	},
	[TEMP_SENSOR_SSD_NEAR] = {
		.temp_host = {
			[EC_TEMP_THRESH_HIGH] = C_TO_K(90),
			[EC_TEMP_THRESH_HALT] = C_TO_K(92),
		},
		.temp_host_release = {
			[EC_TEMP_THRESH_HIGH] = C_TO_K(80),
		},
        .temp_fan_off = C_TO_K(35),
	    .temp_fan_max = C_TO_K(50)
	},
	[TEMP_SENSOR_PCIEX16_NEAR] = {
		.temp_host = {
			[EC_TEMP_THRESH_HIGH] = C_TO_K(90),
			[EC_TEMP_THRESH_HALT] = C_TO_K(92),
		},
		.temp_host_release = {
			[EC_TEMP_THRESH_HIGH] = C_TO_K(80),
		},
        .temp_fan_off = C_TO_K(10),
	    .temp_fan_max = C_TO_K(40)
	},
	[TEMP_SENSOR_ENVIRONMENT] = {
		.temp_host = {
			[EC_TEMP_THRESH_HIGH] = C_TO_K(90),
			[EC_TEMP_THRESH_HALT] = C_TO_K(92),
		},
		.temp_host_release = {
			[EC_TEMP_THRESH_HIGH] = C_TO_K(80),
		},
        .temp_fan_off = C_TO_K(25),
	    .temp_fan_max = C_TO_K(45)
	},
	[TEMP_SENSOR_MEMORY_NEAR] = {
		.temp_host = {
			[EC_TEMP_THRESH_HIGH] = C_TO_K(90),
			[EC_TEMP_THRESH_HALT] = C_TO_K(92),
		},
		.temp_host_release = {
			[EC_TEMP_THRESH_HIGH] = C_TO_K(80),
		},
        .temp_fan_off = C_TO_K(35),
	    .temp_fan_max = C_TO_K(50)
	},
};
BUILD_ASSERT(ARRAY_SIZE(thermal_params) == TEMP_SENSOR_COUNT);

/* TODO: check with real hardware, this is error */
const struct i2c_port_t i2c_ports[] = {
	{
		.name = "smb-ext",
		.port = I2C_PORT_SMB_AUX,
		.kbps = 400,
		.scl = GPIO_SMBCLK_AUX,
		.sda = GPIO_SMBDATA_AUX,
	},
	{
		.name = "tcpc0",
		.port = I2C_PORT_TCPC0,
		.kbps = 400,
		.scl = GPIO_EC_PD_I2C1_SCL,
		.sda = GPIO_EC_PD_I2C1_SDA,
	},
	{
		.name = "thermal",
		.port = I2C_PORT_THERMAL_AP,
		.kbps = 400,
		.scl = GPIO_FCH_SIC,
		.sda = GPIO_FCH_SID,
	},
};
const unsigned int i2c_ports_used = ARRAY_SIZE(i2c_ports);


/******************************************************************************/
/* PWM channels. Must be in the exactly same order as in enum pwm_channel. pwm_init. */
const struct pwm_t pwm_channels[] = {
    [PWM_CH_CPU_FAN] = { 
        .channel = 0,
        .flags = PWM_CONFIG_OPEN_DRAIN,
        .freq = 25000,
    },

    [PWM_CH_SYS_FAN] = { 
		.channel = 1,
		.flags = PWM_CONFIG_OPEN_DRAIN,
		.freq = 25000,
    },

   [PWM_CH_POWER_LED] = {
        .channel = 4,
        .flags = PWM_CONFIG_DSLEEP,
        .freq = 100,
   },

	[PWM_CH_SPKR] = {
		.channel = 6,
		.flags = PWM_CONFIG_DSLEEP,
		.freq = 100,
	},
};
BUILD_ASSERT(ARRAY_SIZE(pwm_channels) == PWM_CH_COUNT);
/******************************************************************************/
/* Physical fans. These are logically separate from pwm_channels. */
const struct fan_conf fan_conf_0 = {
	.flags = FAN_USE_RPM_MODE,
	.ch = MFT_CH_0,	/* Use MFT id to control fan */
	.pgood_gpio = -1,
	.enable_gpio = -1,
};

const struct fan_conf fan_conf_1 = {
	.flags = FAN_USE_RPM_MODE,
	.ch = MFT_CH_1,	/* Use MFT id to control fan */
	.pgood_gpio = -1,
	.enable_gpio = -1,
};

const struct fan_rpm fan_rpm_0 = {
	.rpm_min = 220,
	.rpm_start = 220,
	.rpm_max = 2500,
};

const struct fan_rpm fan_rpm_1 = {
	.rpm_min = 220,
	.rpm_start = 220,
	.rpm_max = 2500,
};

const struct fan_t fans[] = {
	[FAN_CH_0] = { .conf = &fan_conf_0, .rpm = &fan_rpm_0, },
    [FAN_CH_1] = { .conf = &fan_conf_1, .rpm = &fan_rpm_1, },
};
BUILD_ASSERT(ARRAY_SIZE(fans) == FAN_CH_COUNT);

/******************************************************************************/
/* MFT channels. These are logically separate from pwm_channels. */
const struct mft_t mft_channels[] = {
	[MFT_CH_0] = { NPCX_MFT_MODULE_1, TCKC_LFCLK, PWM_CH_CPU_FAN},
    [MFT_CH_1] = { NPCX_MFT_MODULE_2, TCKC_LFCLK, PWM_CH_SYS_FAN},
};
BUILD_ASSERT(ARRAY_SIZE(mft_channels) == MFT_CH_COUNT);
/*
 * We use 11 as the scaling factor so that the maximum mV value below (2761)
 * can be compressed to fit in a uint8_t.
 */
#define THERMISTOR_SCALING_FACTOR 11

/*
 * Values are calculated from the "Resistance VS. Temperature" table on the
 * Murata page for part NCP15WB473F03RC. Vdd=3.3V, R=30.9Kohm.
 */
const struct thermistor_data_pair thermistor_data[] = {
	{ 2761 / THERMISTOR_SCALING_FACTOR, 0},
	{ 2492 / THERMISTOR_SCALING_FACTOR, 10},
	{ 2167 / THERMISTOR_SCALING_FACTOR, 20},
	{ 1812 / THERMISTOR_SCALING_FACTOR, 30},
	{ 1462 / THERMISTOR_SCALING_FACTOR, 40},
	{ 1146 / THERMISTOR_SCALING_FACTOR, 50},
	{ 878 / THERMISTOR_SCALING_FACTOR, 60},
	{ 665 / THERMISTOR_SCALING_FACTOR, 70},
	{ 500 / THERMISTOR_SCALING_FACTOR, 80},
	{ 434 / THERMISTOR_SCALING_FACTOR, 85},
	{ 376 / THERMISTOR_SCALING_FACTOR, 90},
	{ 326 / THERMISTOR_SCALING_FACTOR, 95},
	{ 283 / THERMISTOR_SCALING_FACTOR, 100}
};

const struct thermistor_info thermistor_info = {
	.scaling_factor = THERMISTOR_SCALING_FACTOR,
	.num_pairs = ARRAY_SIZE(thermistor_data),
	.data = thermistor_data,
};
#if 0  // pangu-l PD define
const struct tcpc_config_t tcpc_config[CONFIG_USB_PD_PORT_MAX_COUNT] = {
	{
		.bus_type = EC_BUS_TYPE_I2C,
		.i2c_info = {
			.port = I2C_PORT_TCPC0,
			.addr_flags = RT1715_I2C_ADDR_FLAGS,
		},
		.drv = &rt1715_tcpm_drv,
	},
};
#endif

void pd_power_supply_reset(int port)
{
	/* Disable VBUS */
	//fusb307_power_supply_reset(port);
}

/*******************************************************************************
 * power button
 *
 */

/*
 * b/164921478: On G3->S5, wait for RSMRST_L to be deasserted before asserting
 * PWRBTN_L.
 */
void board_pwrbtn_to_pch(int level)
{
    /* Add delay for G3 exit if asserting PWRBTN_L and S5_PGOOD is low. */
    if (!level && !gpio_get_level(GPIO_S5_PGOOD)) {
        /*
        * From Power Sequence, wait 10 ms for RSMRST_L to rise after
        * S5_PGOOD.
        */
        msleep(10);

        if (!gpio_get_level(GPIO_S5_PGOOD))
            ccprints("Error: pwrbtn S5_PGOOD low");
    }
    
    gpio_set_level(GPIO_PCH_PWRBTN_L, level);
}

static void power_button_record(void)
{
    if(power_button_is_pressed())
    {
        shutdown_cause_record(LOG_ID_SHUTDOWN_0x40);
    }
    else
    {
        shutdown_cause_record(LOG_ID_SHUTDOWN_0x41);
    }
}
DECLARE_HOOK(HOOK_POWER_BUTTON_CHANGE, power_button_record, HOOK_PRIO_DEFAULT);

/*******************************************************************************
 * Board chipset suspend/resume/shutdown/startup
 *
 */
static void board_chipset_resume(void)
{
    wakeup_cause_record(LOG_ID_WAKEUP_0x04);
    ccprints("%s -> %s", __FILE__, __func__);
    return;
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, board_chipset_resume, HOOK_PRIO_DEFAULT);
 
static void board_chipset_suspend(void)
{
    shutdown_cause_record(LOG_ID_SHUTDOWN_0x03);
    ccprints("%s -> %s", __FILE__, __func__);
    return;
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, board_chipset_suspend, HOOK_PRIO_DEFAULT);
 
static void board_chipset_shutdown(void)
{
    uint8_t *mptr = host_get_memmap(EC_MEMMAP_RESET_FLAG);

    if(0xAA == (*mptr))
    {
        (*mptr) = 0;
        shutdown_cause_record(LOG_ID_SHUTDOWN_0x42);
        ccprints("EC reboot......");
        system_reset(SYSTEM_RESET_MANUALLY_TRIGGERED);
    }

    shutdown_cause_record(LOG_ID_SHUTDOWN_0x02);
    ccprints("%s -> %s", __FILE__, __func__);
    return;
}
DECLARE_HOOK(HOOK_CHIPSET_SHUTDOWN, board_chipset_shutdown, HOOK_PRIO_DEFAULT);

static void board_chipset_startup(void)
{
    wakeup_cause_record(LOG_ID_WAKEUP_0x06);
    ccprints("%s -> %s", __FILE__, __func__);
    return;
}
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, board_chipset_startup, HOOK_PRIO_DEFAULT);


static void board_init_config(void)
{
    gpio_config_module(MODULE_HOST_UART, 0);
}
DECLARE_HOOK(HOOK_INIT, board_init_config, HOOK_PRIO_DEFAULT);

void apu_pcie_reset_interrupt(enum gpio_signal signal)
{
    int debounce_sample = 0;

    int first_sample = gpio_get_level(signal);
    usleep(10);
    debounce_sample = gpio_get_level(signal);

    if (first_sample == debounce_sample) {
        gpio_set_level(GPIO_PCIEX16_RST_L, debounce_sample);
        gpio_set_level(GPIO_PCIEX1_RST_L, debounce_sample);
        gpio_set_level(GPIO_M2_2280_SSD1_RST_L, debounce_sample);

        ccprints("apu_pcie_reset, level=%d\n", gpio_get_level(GPIO_APU_PCIE_RST_L));
        return;
    }

    ccprints("Error: apu_pcie_reset glitch, please check");
    return;
}


/*******************************************************************************
 * EC firmware version set
 *
 */
static void ec_oem_version_set(void)
{
    uint8_t *mptr = host_get_memmap(EC_MEMMAP_VERSION_X);

    /* Update ec version to RAM */
    *(mptr+0) = BLD_EC_VERSION_X_HEX;
    *(mptr+1) = BLD_EC_VERSION_YZ_HEX;
    *(mptr+2) = BLD_EC_VERSION_TEST_HEX;

    /* Update board ID to RAM */     
    *(mptr+EC_MEMMAP_GPIO_BOARD_ID) = (uint8_t)system_get_board_version();
    
    /* Update project ID to RAM */
    *(mptr+EC_MEMMAP_GPIO_PROJECT_ID) = (uint8_t)system_get_project_version();
}
DECLARE_HOOK(HOOK_INIT, ec_oem_version_set, HOOK_PRIO_DEFAULT);
DECLARE_HOOK(HOOK_CHIPSET_STARTUP, ec_oem_version_set, HOOK_PRIO_DEFAULT);

