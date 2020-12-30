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
	case TEMP_SENSOR_CHARGER:
		channel = ADC_TEMP_SENSOR_CHARGER;
		break;
	case TEMP_SENSOR_SOC:
		/* thermistor is not powered in G3 */
		if (chipset_in_state(CHIPSET_STATE_HARD_OFF))
			return EC_ERROR_NOT_POWERED;

		channel = ADC_TEMP_SENSOR_SOC;
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
	[ADC_TEMP_SENSOR_CHARGER] = {
		.name = "CHARGER",
		.input_ch = NPCX_ADC_CH2,
		.factor_mul = ADC_MAX_VOLT,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
	},
	[ADC_TEMP_SENSOR_SOC] = {
		.name = "SOC",
		.input_ch = NPCX_ADC_CH3,
		.factor_mul = ADC_MAX_VOLT,
		.factor_div = ADC_READ_MAX + 1,
		.shift = 0,
	},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

const struct temp_sensor_t temp_sensors[] = {
	[TEMP_SENSOR_CHARGER] = {
		.name = "Charger",
		.type = TEMP_SENSOR_TYPE_BOARD,
		.read = board_get_temp,
		.idx = TEMP_SENSOR_CHARGER,
	},
	[TEMP_SENSOR_SOC] = {
		.name = "SOC",
		.type = TEMP_SENSOR_TYPE_BOARD,
		.read = board_get_temp,
		.idx = TEMP_SENSOR_SOC,
	},
	[TEMP_SENSOR_CPU] = {
		.name = "CPU",
		.type = TEMP_SENSOR_TYPE_CPU,
		.read = sb_tsi_get_val,
		.idx = 0,
	},
};
BUILD_ASSERT(ARRAY_SIZE(temp_sensors) == TEMP_SENSOR_COUNT);

__overridable struct ec_thermal_config thermal_params[TEMP_SENSOR_COUNT] = {
	[TEMP_SENSOR_CHARGER] = {
		.temp_host = {
			[EC_TEMP_THRESH_HIGH] = C_TO_K(90),
			[EC_TEMP_THRESH_HALT] = C_TO_K(92),
		},
		.temp_host_release = {
			[EC_TEMP_THRESH_HIGH] = C_TO_K(80),
		}
	},
	[TEMP_SENSOR_SOC] = {
		.temp_host = {
			[EC_TEMP_THRESH_HIGH] = C_TO_K(90),
			[EC_TEMP_THRESH_HALT] = C_TO_K(92),
		},
		.temp_host_release = {
			[EC_TEMP_THRESH_HIGH] = C_TO_K(80),
		}
	},
	[TEMP_SENSOR_CPU] = {
		.temp_host = {
			[EC_TEMP_THRESH_HIGH] = C_TO_K(90),
			[EC_TEMP_THRESH_HALT] = C_TO_K(92),
		},
		.temp_host_release = {
			[EC_TEMP_THRESH_HIGH] = C_TO_K(80),
		}
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

const struct pwm_t pwm_channels[] = {
	[PWM_CH_KBLIGHT] = {
		.channel = 3,
		.flags = PWM_CONFIG_DSLEEP,
		.freq = 100,
	},
};
BUILD_ASSERT(ARRAY_SIZE(pwm_channels) == PWM_CH_COUNT);

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

/*****************************************************************************
 * Board suspend / resume
 */

static void board_chipset_resume(void)
{
	return;
}
DECLARE_HOOK(HOOK_CHIPSET_RESUME, board_chipset_resume, HOOK_PRIO_DEFAULT);

static void board_chipset_suspend(void)
{
	return;
}
DECLARE_HOOK(HOOK_CHIPSET_SUSPEND, board_chipset_suspend, HOOK_PRIO_DEFAULT);

/*****************************************************************************
 * USB-C
 */


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

		ccprints("apu_pcie_reset");
		return;
	}

	ccprints("Error: apu_pcie_reset glitch, please check");
	return;
}


static void ec_oem_version_set(void)
{
    uint8_t *mptr = host_get_memmap(EC_MEMMAP_VERSION_X);

    *(mptr+0) = BLD_EC_VERSION_X_HEX;
    *(mptr+1) = BLD_EC_VERSION_YZ_HEX;
    *(mptr+2) = BLD_EC_VERSION_TEST_HEX;
}
DECLARE_HOOK(HOOK_INIT, ec_oem_version_set, HOOK_PRIO_DEFAULT);

