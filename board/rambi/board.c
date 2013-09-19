/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
/* EC for Rambi board configuration */

#include "adc.h"
#include "backlight.h"
#include "chip_temp_sensor.h"
#include "chipset_haswell.h"
#include "chipset_x86_common.h"
#include "common.h"
#include "ec_commands.h"
#include "extpower.h"
#include "gpio.h"
#include "host_command.h"
#include "i2c.h"
#include "keyboard_scan.h"
#include "lid_switch.h"
#include "lm4_adc.h"
#include "peci.h"
#include "power_button.h"
#include "pwm.h"
#include "pwm_data.h"
#include "registers.h"
#include "switch.h"
#include "temp_sensor.h"
#include "temp_sensor_g781.h"
#include "thermal.h"
#include "timer.h"
#include "util.h"

/* GPIO signal list.  Must match order from enum gpio_signal. */
const struct gpio_info gpio_list[] = {
	/* Inputs with interrupt handlers are first for efficiency */
	{"POWER_BUTTON_L",       LM4_GPIO_A, (1<<2), GPIO_INT_BOTH,
	 power_button_interrupt},
	{"LID_OPEN",             LM4_GPIO_A, (1<<3), GPIO_INT_BOTH,
	 lid_interrupt},
	{"AC_PRESENT",           LM4_GPIO_H, (1<<3), GPIO_INT_BOTH,
	 extpower_interrupt},
	{"PCH_SLP_S3_L",         LM4_GPIO_G, (1<<7), GPIO_INT_BOTH|GPIO_PULL_UP,
	 x86_interrupt},
	{"PCH_SLP_S4_L",         LM4_GPIO_H, (1<<1), GPIO_INT_BOTH|GPIO_PULL_UP,
	 x86_interrupt},
	{"PP1050_PGOOD",         LM4_GPIO_H, (1<<4), GPIO_INT_BOTH,
	 x86_interrupt},
	{"PP3300_PCH_PGOOD",     LM4_GPIO_C, (1<<4), GPIO_INT_BOTH,
	 x86_interrupt},
	{"PP5000_PGOOD",         LM4_GPIO_N, (1<<0), GPIO_INT_BOTH,
	 x86_interrupt},
	{"S5_PGOOD",             LM4_GPIO_G, (1<<0), GPIO_INT_BOTH,
	 x86_interrupt},
	{"VCORE_PGOOD",          LM4_GPIO_C, (1<<6), GPIO_INT_BOTH,
	 x86_interrupt},
	{"WP_L",                 LM4_GPIO_A, (1<<4), GPIO_INT_BOTH|GPIO_PULL_UP,
	 switch_interrupt},

	/* Other inputs */
	{"BOARD_VERSION1",       LM4_GPIO_Q, (1<<5), GPIO_INPUT, NULL},
	{"BOARD_VERSION2",       LM4_GPIO_Q, (1<<6), GPIO_INPUT, NULL},
	{"BOARD_VERSION3",       LM4_GPIO_Q, (1<<7), GPIO_INPUT, NULL},
	{"PCH_SLP_SX_L",         LM4_GPIO_G, (1<<3), GPIO_INPUT|GPIO_PULL_UP,
	 NULL},
	{"PCH_SUS_STAT_L",       LM4_GPIO_G, (1<<6), GPIO_INPUT, NULL},
	{"PCH_SUSPWRDNACK",      LM4_GPIO_G, (1<<2), GPIO_INPUT|GPIO_PULL_UP,
	 NULL},
	{"PP1000_S0IX_PGOOD",    LM4_GPIO_H, (1<<6), GPIO_INPUT, NULL},
	{"USB1_OC_L",            LM4_GPIO_E, (1<<7), GPIO_INPUT, NULL},
	{"USB2_OC_L",            LM4_GPIO_E, (1<<0), GPIO_INPUT, NULL},

	/* Outputs; all unasserted by default except for reset signals */
	{"CPU_PROCHOT",          LM4_GPIO_B, (1<<5), GPIO_OUT_LOW, NULL},
	{"ENABLE_BACKLIGHT",     LM4_GPIO_M, (1<<7), GPIO_ODR_HIGH, NULL},
	{"ENABLE_TOUCHPAD",      LM4_GPIO_N, (1<<1), GPIO_OUT_LOW, NULL},
	{"ENTERING_RW",          LM4_GPIO_D, (1<<6), GPIO_OUT_LOW, NULL},
	{"LPC_CLKRUN_L",         LM4_GPIO_M, (1<<2), GPIO_ODR_HIGH, NULL},
	{"PCH_CORE_PWROK",       LM4_GPIO_F, (1<<5), GPIO_OUT_LOW, NULL},
	{"PCH_PWRBTN_L",         LM4_GPIO_H, (1<<0), GPIO_OUT_HIGH, NULL},
	{"PCH_RCIN_L",           LM4_GPIO_F, (1<<3), GPIO_ODR_LOW, NULL},
	{"PCH_RSMRST_L",         LM4_GPIO_F, (1<<1), GPIO_OUT_LOW, NULL},
	{"PCH_SMI_L",            LM4_GPIO_F, (1<<4), GPIO_ODR_HIGH, NULL},
	{"PCH_SOC_OVERRIDE_L",   LM4_GPIO_G, (1<<1), GPIO_OUT_HIGH, NULL},
	{"PCH_SYS_PWROK",        LM4_GPIO_H, (1<<2), GPIO_OUT_LOW, NULL},
	{"PCH_WAKE_L",           LM4_GPIO_F, (1<<0), GPIO_OUT_HIGH, NULL},
	{"PP1350_EN",            LM4_GPIO_H, (1<<5), GPIO_OUT_LOW, NULL},
	{"PP3300_DX_EN",         LM4_GPIO_J, (1<<2), GPIO_OUT_LOW, NULL},
	{"PP3300_LTE_EN",        LM4_GPIO_D, (1<<2), GPIO_OUT_LOW, NULL},
	{"PP3300_WLAN_EN",       LM4_GPIO_J, (1<<0), GPIO_OUT_LOW, NULL},
	{"PP5000_EN",            LM4_GPIO_H, (1<<7), GPIO_OUT_LOW, NULL},
	{"PPSX_EN",              LM4_GPIO_L, (1<<6), GPIO_OUT_LOW, NULL},
	{"SUSP_VR_EN",           LM4_GPIO_C, (1<<7), GPIO_OUT_LOW, NULL},
	{"TOUCHSCREEN_RESET_L",  LM4_GPIO_N, (1<<7), GPIO_OUT_LOW, NULL},
	{"USB_CTL1",             LM4_GPIO_E, (1<<6), GPIO_OUT_LOW, NULL},
	{"USB_ILIM_SEL",         LM4_GPIO_E, (1<<5), GPIO_OUT_LOW, NULL},
	{"USB1_ENABLE",          LM4_GPIO_E, (1<<4), GPIO_OUT_LOW, NULL},
	{"USB2_ENABLE",          LM4_GPIO_D, (1<<5), GPIO_OUT_LOW, NULL},
	{"VCORE_EN",             LM4_GPIO_C, (1<<5), GPIO_OUT_LOW, NULL},
	{"WLAN_OFF_L",           LM4_GPIO_J, (1<<4), GPIO_OUT_LOW, NULL},
};
BUILD_ASSERT(ARRAY_SIZE(gpio_list) == GPIO_COUNT);

/* Pins with alternate functions */
const struct gpio_alt_func gpio_alt_funcs[] = {
	{GPIO_A, 0x03, 1, MODULE_UART},			/* UART0 */
	{GPIO_B, 0x04, 3, MODULE_I2C},			/* I2C0 SCL */
	{GPIO_B, 0x08, 3, MODULE_I2C, GPIO_OPEN_DRAIN},	/* I2C0 SDA */
	{GPIO_B, 0x40, 3, MODULE_I2C},			/* I2C5 SCL */
	{GPIO_B, 0x80, 3, MODULE_I2C, GPIO_OPEN_DRAIN},	/* I2C5 SDA */
	{GPIO_D, 0x0f, 2, MODULE_SPI},			/* SPI1 */
	{GPIO_L, 0x3f, 15, MODULE_LPC},			/* LPC */
	{GPIO_M, 0x33, 15, MODULE_LPC},			/* LPC */
	{GPIO_N, 0x50, 1, MODULE_PWM_LED},		/* Power LEDs */
};
const int gpio_alt_funcs_count = ARRAY_SIZE(gpio_alt_funcs);

/* x86 signal list.  Must match order of enum x86_signal. */
const struct x86_signal_info x86_signal_list[] = {
	{GPIO_PP1050_PGOOD,      1, "PGOOD_PP1050"},
	{GPIO_PP3300_PCH_PGOOD,  1, "PGOOD_PP3300_PCH"},
	{GPIO_PP5000_PGOOD,      1, "PGOOD_PP5000"},
	{GPIO_S5_PGOOD,          1, "PGOOD_S5"},
	{GPIO_VCORE_PGOOD,       1, "PGOOD_VCORE"},
	{GPIO_PP1000_S0IX_PGOOD, 1, "PGOOD_PP1000_S0IX"},
	{GPIO_PCH_SLP_S3_L,      1, "SLP_S3#_DEASSERTED"},
	{GPIO_PCH_SLP_S4_L,      1, "SLP_S4#_DEASSERTED"},
	{GPIO_PCH_SLP_SX_L,      1, "SLP_SX#_DEASSERTED"},
	{GPIO_PCH_SUS_STAT_L,    0, "SUS_STAT#_ASSERTED"},
	{GPIO_PCH_SUSPWRDNACK,   1, "SUSPWRDNACK_ASSERTED"},
};
BUILD_ASSERT(ARRAY_SIZE(x86_signal_list) == X86_SIGNAL_COUNT);

/* ADC channels. Must be in the exactly same order as in enum adc_channel. */
const struct adc_t adc_channels[] = {
	/* EC internal temperature is calculated by
	 * 273 + (295 - 450 * ADC_VALUE / ADC_READ_MAX) / 2
	 * = -225 * ADC_VALUE / ADC_READ_MAX + 420.5
	 */
	{"ECTemp", LM4_ADC_SEQ0, -225, ADC_READ_MAX, 420,
	 LM4_AIN_NONE, 0x0e /* TS0 | IE0 | END0 */, 0, 0},

	/* IOUT == ICMNT is on PE3/AIN0 */
	/* We have 0.01-ohm resistors, and IOUT is 20X the differential
	 * voltage, so 1000mA ==> 200mV.
	 * ADC returns 0x000-0xFFF, which maps to 0.0-3.3V (as configured).
	 * mA = 1000 * ADC_VALUE / ADC_READ_MAX * 3300 / 200
	 */
	{"ChargerCurrent", LM4_ADC_SEQ1, 33000, ADC_READ_MAX * 2, 0,
	 LM4_AIN(0), 0x06 /* IE0 | END0 */, LM4_GPIO_E, (1<<3)},
};
BUILD_ASSERT(ARRAY_SIZE(adc_channels) == ADC_CH_COUNT);

/* PWM channels */
const struct pwm_t pwm_channels[] = {
	[PWM_CH_LED_GREEN] = {FAN_CH_LED_GREEN, 0},
	[PWM_CH_LED_RED] = {FAN_CH_LED_RED, 0},
};
BUILD_ASSERT(ARRAY_SIZE(pwm_channels) == PWM_CH_COUNT);

/* I2C ports */
const struct i2c_port_t i2c_ports[] = {
	/* Note: battery and charger share a port.  Only include it once in
	 * this list so we don't double-initialize it. */
	{"batt_chg", I2C_PORT_BATTERY,  100},
	{"thermal",  I2C_PORT_THERMAL,  100},
};
BUILD_ASSERT(ARRAY_SIZE(i2c_ports) == I2C_PORTS_USED);

/* Temperature sensors data; must be in same order as enum temp_sensor_id. */
const struct temp_sensor_t temp_sensors[] = {
	{"ECInternal", TEMP_SENSOR_TYPE_BOARD, chip_temp_sensor_get_val, 0, 4},
	{"G781Internal", TEMP_SENSOR_TYPE_BOARD, g781_get_val,
		G781_IDX_INTERNAL, 4},
	{"G781External", TEMP_SENSOR_TYPE_BOARD, g781_get_val,
		G781_IDX_EXTERNAL, 4},
};
BUILD_ASSERT(ARRAY_SIZE(temp_sensors) == TEMP_SENSOR_COUNT);

/* Thermal limits for each temp sensor. All temps are in degrees K. Must be in
 * same order as enum temp_sensor_id. To always ignore any temp, use 0.
 */
struct ec_thermal_config thermal_params[] = {
	{{0, 0, 0}, 0, 0},
	{{0, 0, 0}, 0, 0},
	{{0, 0, 0}, 0, 0},
};
BUILD_ASSERT(ARRAY_SIZE(thermal_params) == TEMP_SENSOR_COUNT);

/**
 * Perform necessary actions on host wake events.
 */
void board_process_wake_events(uint32_t active_wake_events)
{
	uint32_t power_button_mask;

	power_button_mask = EC_HOST_EVENT_MASK(EC_HOST_EVENT_POWER_BUTTON);

	/* If there are other events aside from the power button press drive
	 * the wake pin. Otherwise ensure it is high. */
	if (active_wake_events & ~power_button_mask)
		gpio_set_level(GPIO_PCH_WAKE_L, 0);
	else
		gpio_set_level(GPIO_PCH_WAKE_L, 1);
}

/**
 * Board-specific g781 power state.
 */
int board_g781_has_power(void)
{
	return gpio_get_level(GPIO_PP3300_DX_EN);
}
