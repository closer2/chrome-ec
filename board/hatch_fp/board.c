/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "common.h"
#include "console.h"
#include "fpsensor_detect.h"
#include "gpio.h"
#include "hooks.h"
#include "registers.h"
#include "spi.h"
#include "system.h"
#include "task.h"
#include "usart_host_command.h"
#include "util.h"

/*
 * Some platforms have a broken SLP_S0_L signal (stuck to 0 in S0)
 * if set, ignore it and only uses SLP_S3_L for the AP state.
 */
static bool broken_slp_s0;

/**
 * Disable restricted commands when the system is locked.
 *
 * @see console.h system.c
 */
int console_is_restricted(void)
{
	return system_is_locked();
}

#ifndef HAS_TASK_FPSENSOR
void fps_event(enum gpio_signal signal)
{
}
#endif

static void ap_deferred(void)
{
	/*
	 * in S3:   SLP_S3_L is 0 and SLP_S0_L is X.
	 * in S0ix: SLP_S3_L is 1 and SLP_S0_L is 0.
	 * in S0:   SLP_S3_L is 1 and SLP_S0_L is 1.
	 * in S5/G3, the FP MCU should not be running.
	 */
	int running = gpio_get_level(GPIO_PCH_SLP_S3_L)
			&& (gpio_get_level(GPIO_PCH_SLP_S0_L) || broken_slp_s0);

	if (running) { /* S0 */
		disable_sleep(SLEEP_MASK_AP_RUN);
		hook_notify(HOOK_CHIPSET_RESUME);
	} else { /* S0ix/S3 */
		hook_notify(HOOK_CHIPSET_SUSPEND);
		enable_sleep(SLEEP_MASK_AP_RUN);
	}
}
DECLARE_DEFERRED(ap_deferred);

/* PCH power state changes */
void slp_event(enum gpio_signal signal)
{
	hook_call_deferred(&ap_deferred_data, 0);
}

#include "gpio_list.h"

/* SPI devices */
const struct spi_device_t spi_devices[] = {
	/* Fingerprint sensor (SCLK at 4Mhz) */
	{ .port = CONFIG_SPI_FP_PORT, .div = 3, .gpio_cs = GPIO_SPI2_NSS }
};
const unsigned int spi_devices_used = ARRAY_SIZE(spi_devices);

static void configure_fp_sensor_spi(void)
{
	/* Configure SPI GPIOs */
	gpio_config_module(MODULE_SPI_MASTER, 1);

	/* Set all SPI master signal pins to very high speed: B12/13/14/15 */
	STM32_GPIO_OSPEEDR(GPIO_B) |= 0xff000000;

	/* Enable clocks to SPI2 module (master) */
	STM32_RCC_APB1ENR |= STM32_RCC_PB1_SPI2;

	spi_enable(CONFIG_SPI_FP_PORT, 1);
}

/* Initialize board. */
static void board_init(void)
{
	enum fp_transport_type ret_transport = get_fp_transport_type();

	/* Run until the first S3 entry */
	disable_sleep(SLEEP_MASK_AP_RUN);

	/* Configure and enable SPI as master for FP sensor */
	configure_fp_sensor_spi();

	ccprints("TRANSPORT_SEL: %s",
		fp_transport_type_to_str(ret_transport));

	/* Initialize transport based on bootstrap */
	switch (ret_transport) {

	case FP_TRANSPORT_TYPE_UART:
		/* Check if CONFIG_USART_HOST_COMMAND is enabled. */
		if (IS_ENABLED(CONFIG_USART_HOST_COMMAND))
			usart_host_command_init();
		else
			ccprints("ERROR: UART not supported in fw build.");

		/* Disable SPI interrupt to disable SPI transport layer */
		gpio_disable_interrupt(GPIO_SPI1_NSS);

		/*
		 * The Zork variants currently have a broken SLP_S0_L signal
		 * (stuck to 0 in S0). For now, unconditionally ignore it here
		 * as they are the only UART users and the AP has no S0ix state.
		 * TODO(b/174695987) once the RW AP firmware has been updated
		 * on all those machines, remove this workaround.
		 */
		broken_slp_s0 = true;
		break;

	case FP_TRANSPORT_TYPE_SPI:
		/* SPI transport is enabled. SPI1_NSS interrupt will process
		 * incoming request/
		 */
		break;
	default:
		ccprints("ERROR: Selected transport is not valid.");
	}

	ccprints("TRANSPORT_SEL: %s",
		fp_transport_type_to_str(get_fp_transport_type()));

	/* Enable interrupt on PCH power signals */
	gpio_enable_interrupt(GPIO_PCH_SLP_S3_L);
	gpio_enable_interrupt(GPIO_PCH_SLP_S0_L);

	/* enable the SPI slave interface if the PCH is up */
	hook_call_deferred(&ap_deferred_data, 0);
}
DECLARE_HOOK(HOOK_INIT, board_init, HOOK_PRIO_DEFAULT);
