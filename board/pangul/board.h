/* Copyright 2020 by Bitland Co.,Ltd. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* PanGuL board configuration */

#ifndef __CROS_EC_BOARD_H
#define __CROS_EC_BOARD_H

/*------------------------------------------------------------------------------
*   ODM EC version define
*   BLD_EC_VERSION_X :
*               0:non-shipping version
*               1:shipping version
*
*   BLD_EC_VERSION_YZ :
*            if VERSION_X=0, EVT(01~4F)/DVT(50~7F)/Reserved(80~FF)
*            if VERSION_X=1, Just increase in order
*
*   BLD_EC_VERSION_TEST :
*            EC test version for ODM debug
------------------------------------------------------------------------------*/
#define BLD_EC_VERSION_X        "0"
#define BLD_EC_VERSION_YZ       "01"
#define BLD_EC_VERSION_TEST     "05"

#define BLD_EC_VERSION_X_HEX    0x00
#define BLD_EC_VERSION_YZ_HEX   0x01
#define BLD_EC_VERSION_TEST_HEX 0x05


/*------------------------------------------------------------------------------
* NPCX7 config
------------------------------------------------------------------------------*/
#define NPCX_UART_MODULE2 1                 /* GPIO64/65 are used as UART pins. */
#define NPCX_TACH_SEL2    0                 /* 0:GPIO40/73 1:GPIO93/A6 as TACH */
#define NPCX7_PWM1_SEL    0                 /* GPIO C2 is not used as PWM1. */

/* Remove after bringup */
/*#define CONFIG_BRINGUP*/

/* Internal SPI flash on NPCX7 */
#define CONFIG_FLASH_SIZE (512 * 1024)      /* For flash size define*/
#define CONFIG_SPI_FLASH_REGS               /* Support SPI flash protection register translation */
#define CONFIG_SPI_FLASH_W25Q40             /* Internal SPI flash type. */

#define CC_DEFAULT     (CC_ALL & ~(CC_MASK(CC_HOSTCMD)))

/*
 * Enable 1 slot of secure temporary storage to support
 * suspend/resume with read/write memory training.
 */
#define CONFIG_VSTORE
#define CONFIG_VSTORE_SLOT_COUNT 1

#define CONFIG_ADC
#define CONFIG_PRESERVE_LOGS                /* init UART TX buffer and save some log*/
#define CONFIG_CMD_AP_RESET_LOG             /* reset UART TX buffer*/

/*
 * Use PSL (Power Switch Logic) for hibernating. It turns off VCC power rail
 * for ultra-low power consumption and uses PSL inputs rely on VSBY power rail
 * to wake up ec and the whole system.
 */
#define CONFIG_HIBERNATE_PSL

#define CONFIG_I2C
#define CONFIG_I2C_CONTROLLER
#define CONFIG_I2C_UPDATE_IF_CHANGED
#define CONFIG_LOW_POWER_IDLE
#define CONFIG_LTO
#define CONFIG_PWM
#define CONFIG_SOFTWARE_WATCHDOG

/* Increase length of history buffer for port80 messages. */
#undef CONFIG_PORT80_HISTORY_LEN
#define CONFIG_PORT80_HISTORY_LEN 256

/* Increase console output buffer since we have the RAM available. */
#undef CONFIG_UART_TX_BUF_SIZE
#define CONFIG_UART_TX_BUF_SIZE 4096

/*------------------------------------------------------------------------------
* Board config
------------------------------------------------------------------------------*/
/* TODO: need confirm TEMP_SENSOR AND TYPE*/
#define CONFIG_TEMP_SENSOR                  /* Compile common code for temperature sensor support */
#define CONFIG_THERMISTOR_NCP15WB           /* Support particular thermistors */
#define CONFIG_TEMP_SENSOR_SB_TSI           /* SB_TSI sensor, on I2C bus */

#undef CONFIG_LID_SWITCH                    /* no lid switch */

#define CONFIG_BOARD_VERSION_GPIO           /* For board version define*/


#define CONFIG_FLASH_LOG_OEM                /* For flash write log function*/
#define CONFIG_CMD_FLASH                    /* For flash console command function*/
#define CONFIG_CMD_RTC                      /* For RTC console command function*/
#define CONFIG_HOSTCMD_RTC                  /* For host update EC RTC*/

#define CONFIG_HOSTCMD_LPC                  /* For host command interface over LPC bus*/
#undef  CONFIG_UART_HOST                    /* Turn on serial port */

/* TODO: remove VBOOT option */
/*#define CONFIG_VBOOT_EFS2
#define CONFIG_VBOOT_HASH
#define CONFIG_CRC8*/                       /* This board no need to verify boot*/

#define CONFIG_CHIPSET_RENOIR               /* For select renoir platform power sequence when build(ec/power/build.mk)*/
#define CONFIG_CHIPSET_CAN_THROTTLE         /* Support chipset throttling */
#define CONFIG_CHIPSET_RESET_HOOK           /* Enable chipset reset hook, requires a deferrable function */

#define CONFIG_POWER_COMMON                 /* For config common power sequence*/
#define CONFIG_POWER_SHUTDOWN_PAUSE_IN_S5   /* Seems pointless*/
#define CONFIG_POWER_BUTTON                 /* Compile common code to support power button debouncing */
#define CONFIG_POWER_BUTTON_X86             /* Support sending the power button signal to x86 chipsets */
#define CONFIG_POWER_BUTTON_IGNORE_LID      /* Allow the power button to send events while the lid is closed */
#define CONFIG_POWER_BUTTON_TO_PCH_CUSTOM   /* Board provides board_pwrbtn_to_pch function*/
#define CONFIG_THROTTLE_AP                  /* Compile common code for throttling the CPU based on the temp sensors */
#define CONFIG_CPU_PROCHOT_ACTIVE_LOW       /* On x86 systems, define this option if the CPU_PROCHOT signal is active low.*/

/*#undef  CONFIG_EXTPOWER_DEBOUNCE_MS
#define CONFIG_EXTPOWER_DEBOUNCE_MS 200
#define CONFIG_EXTPOWER_GPIO*/
#undef CONFIG_EXTPOWER                      /* This board is desktop, NO AC status, undefine it*/
#undef  CONFIG_FANS
#define CONFIG_FANS 2 






/* TODO: config fans */
/* #ifdef VARIANT_ZORK_TREMBYLE
	#define CONFIG_FANS FAN_CH_COUNT
	#undef CONFIG_FAN_INIT_SPEED
	#define CONFIG_FAN_INIT_SPEED 50
#endif */

/* TODO: add back led */
/* #define CONFIG_LED_COMMON */
/* #define CONFIG_CMD_LEDTEST */
/* #define CONFIG_LED_ONOFF_STATES */

/*
 * On power-on, H1 releases the EC from reset but then quickly asserts and
 * releases the reset a second time. This means the EC sees 2 resets:
 * (1) power-on reset, (2) reset-pin reset. This config will
 * allow the second reset to be treated as a power-on.
 */
/* TODO: review this, we do not need this in theory */
/* #define CONFIG_BOARD_RESET_AFTER_POWER_ON */

/* Disable PD as a whole, reopen it when ready */
/* TODO: handle PD */
#if 0
/*
 * USB ID
 *
 * This is allocated specifically for Zork
 * http://google3/hardware/standards/usb/
 */
#define CONFIG_USB_PID 0x5040

#define CONFIG_USB_PD_REV30

/* Enable the TCPMv2 PD stack */
#define CONFIG_USB_PD_TCPMV2

#ifndef CONFIG_USB_PD_TCPMV2
	#define CONFIG_USB_PD_TCPMV1
#else
	#define CONFIG_USB_PD_DECODE_SOP
	#define CONFIG_USB_DRP_ACC_TRYSRC

	 /* Enable TCPMv2 Fast Role Swap */
	 /* Turn off until FRSwap is working */
	#undef CONFIG_USB_PD_FRS_TCPC
#endif

#define CONFIG_HOSTCMD_PD_CONTROL
#define CONFIG_CMD_TCPC_DUMP
#define CONFIG_USB_CHARGER
#define CONFIG_USB_POWER_DELIVERY
#define CONFIG_USB_PD_ALT_MODE
#define CONFIG_USB_PD_ALT_MODE_DFP
#define CONFIG_USB_PD_COMM_LOCKED
#define CONFIG_USB_PD_DISCHARGE_TCPC
#define CONFIG_USB_PD_DP_HPD_GPIO
#ifdef VARIANT_ZORK_TREMBYLE
/*
 * Use a custom HPD function that supports HPD on IO expander.
 * TODO(b/165622386) remove this when HPD is on EC GPIO.
 */
#	define CONFIG_USB_PD_DP_HPD_GPIO_CUSTOM
#endif
#define CONFIG_USB_PD_DUAL_ROLE
#define CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE
#define CONFIG_USB_PD_LOGGING
#define CONFIG_USB_PD_MAX_SINGLE_SOURCE_CURRENT TYPEC_RP_3A0
#define CONFIG_USB_PD_TCPC_LOW_POWER
#define CONFIG_USB_PD_TCPM_MUX
#define CONFIG_USB_PD_TCPM_NCT38XX
#define CONFIG_USB_PD_TCPM_TCPCI
#define CONFIG_USB_PD_TRY_SRC
#define CONFIG_USB_PD_VBUS_DETECT_TCPC
#define CONFIG_USBC_PPC
#define CONFIG_USBC_PPC_SBU
#define CONFIG_USBC_PPC_AOZ1380
#define CONFIG_USBC_RETIMER_PI3HDX1204
#define CONFIG_USBC_SS_MUX
#define CONFIG_USBC_SS_MUX_DFP_ONLY
#define CONFIG_USBC_VCONN
#define CONFIG_USBC_VCONN_SWAP
#define CONFIG_USB_MUX_AMD_FP5

#if defined(VARIANT_ZORK_TREMBYLE)
	#define CONFIG_USB_PD_PORT_MAX_COUNT 2
	#define CONFIG_USBC_PPC_NX20P3483
	#define CONFIG_USBC_RETIMER_PS8802
	#define CONFIG_USBC_RETIMER_PS8818
	#define CONFIG_IO_EXPANDER_PORT_COUNT USBC_PORT_COUNT
	#define CONFIG_USB_MUX_RUNTIME_CONFIG
	/* USB-A config */
	#define GPIO_USB1_ILIM_SEL IOEX_USB_A0_CHARGE_EN_L
	#define GPIO_USB2_ILIM_SEL IOEX_USB_A1_CHARGE_EN_DB_L
	/* PS8818 RX Input Termination - default value */
	#define ZORK_PS8818_RX_INPUT_TERM PS8818_RX_INPUT_TERM_112_OHM
#elif defined(VARIANT_ZORK_DALBOZ)
	#define CONFIG_IO_EXPANDER_PORT_COUNT IOEX_PORT_COUNT
#endif
#endif /* disable PD as a whole, reopen it next */

#if 0 // pangu-l PD define

#define CONFIG_USB
#define CONFIG_USB_PID 0x1234
/*#define CONFIG_USB_CONSOLE*/

#define CONFIG_USB_POWER_DELIVERY
#define CONFIG_USB_PD_TCPMV1
#define CONFIG_USB_PD_PORT_MAX_COUNT 1
#define CONFIG_USB_PD_TCPM_TCPCI
#define CONFIG_USB_PD_VBUS_DETECT_TCPC
#define CONFIG_USB_PD_REV30
#define CONFIG_USB_PD_DECODE_SOP
#define CONFIG_USBC_VCONN
#define CONFIG_USB_PD_TCPM_RT1715
#endif

/* TODO: Maybe these definitions should be cleard */
#define PD_POWER_SUPPLY_TURN_ON_DELAY	30000 /* us */
#define PD_POWER_SUPPLY_TURN_OFF_DELAY	30000 /* us */
#define PD_VCONN_SWAP_DELAY		5000 /* us */

#define PD_OPERATING_POWER_MW	15000
#define PD_MAX_POWER_MW		65000
#define PD_MAX_CURRENT_MA	3250
#define PD_MAX_VOLTAGE_MV	20000

/* Round up 3250 max current to multiple of 128mA for ISL9241 AC prochot. */
#define ZORK_AC_PROCHOT_CURRENT_MA 3328

/*
 * EC will boot AP to depthcharge if: (BAT >= 4%) || (AC >= 50W)
 * CONFIG_CHARGER_LIMIT_* is not set, so there is no additional restriction on
 * Depthcharge to boot OS.
 */
#define CONFIG_CHARGER_MIN_BAT_PCT_FOR_POWER_ON			4
#define CONFIG_CHARGER_MIN_POWER_MW_FOR_POWER_ON		50000



/* USBA0 not control, SMBCLK/SMBDATA_AUX connect to PORT0_0 */
/* We only have one PD TCPC connect to PORT1_0 */
#define I2C_PORT_SMB_AUX	NPCX_I2C_PORT0_0
#define I2C_PORT_TCPC0		NPCX_I2C_PORT1_0
#define I2C_PORT_THERMAL_AP	NPCX_I2C_PORT3_0


/* TODO: need confirm with real hardware */
/* GPIO mapping from board specific name to EC common name. */
#define CONFIG_SCI_GPIO                 GPIO_EC_FCH_SCI_ODL
#define GPIO_CPU_PROCHOT                GPIO_PROCHOT_ODL
#define GPIO_EC_INT_L                   GPIO_EC_AP_INT_ODL
#define GPIO_PCH_PWRBTN_L               GPIO_EC_FCH_PWR_BTN_L
#define GPIO_PCH_RSMRST_L               GPIO_EC_FCH_RSMRST_L
#define GPIO_PCH_SLP_S3_L               GPIO_SLP_S3_L
#define GPIO_PCH_SLP_S5_L               GPIO_SLP_S5_L
#define GPIO_PCH_SYS_PWROK              GPIO_EC_FCH_PWROK
#define GPIO_POWER_BUTTON_L             GPIO_EC_PWR_BTN_ODL
#define GPIO_S5_PGOOD			        GPIO_SYSTEM_ALW_PG

#define GPIO_BOARD_VERSION1             GPIO_EC_BRD_ID0
#define GPIO_BOARD_VERSION2             GPIO_EC_BRD_ID1
#define GPIO_BOARD_VERSION3             GPIO_EC_BRD_ID2
#define GPIO_PROJECT_VERSION1           GPIO_EC_PROJECT_ID0
#define GPIO_PROJECT_VERSION2           GPIO_EC_PROJECT_ID1
#define GPIO_PROJECT_VERSION3           GPIO_EC_PROJECT_ID2


/* We can select CONFIG_WP_ALWAYS for independent on gpio, Or
 * We can use UNIMPLEMENTED(WP_L), but they have different
 * meaning.
 */
/* #define GPIO_WP_L                    GPIO_EC_WP_L */

#ifndef __ASSEMBLER__

#include "gpio_signal.h"
#include "math_util.h"
#include "power/renoir.h"
#include "registers.h"

/* TODO: need confirm with real hardware */
enum pwm_channel {
    PWM_CH_CPU_FAN = 0,
    PWM_CH_SYS_FAN,
    PWM_CH_SPKR,
    PWM_CH_POWER_LED,
    PWM_CH_COUNT
};

enum fan_channel {
	FAN_CH_0 = 0,
    FAN_CH_1,
	/* Number of FAN channels */
	FAN_CH_COUNT,
};

enum mft_channel {
	MFT_CH_0, /* TA1 */
    MFT_CH_1, /* TA2 */
	/* Number of MFT channels */
	MFT_CH_COUNT
};

#ifdef VARIANT_ZORK_TREMBYLE
enum usbc_port {
	USBC_PORT_C0 = 0,
	USBC_PORT_C1,
	USBC_PORT_COUNT
};
#endif

enum sensor_id {
	LID_ACCEL,
	BASE_ACCEL,
	BASE_GYRO,
	SENSOR_COUNT,
};

/* TODO: need confirm with real hardware */
enum adc_channel {
    ADC_SENSOR_SOC_NEAR = 0,
    ADC_SENSOR_SSD_NEAR,
    ADC_SENSOR_PCIEX16_NEAR,
    ADC_SENSOR_ENVIRONMENT,
    ADC_SENSOR_MEMORY_NEAR,
	ADC_CH_COUNT
};

/* TODO: need confirm with real hardware */
enum temp_sensor_id {
        TEMP_SENSOR_SOC_CORE = 0,
        TEMP_SENSOR_SOC_NEAR,
        TEMP_SENSOR_SSD_NEAR,
        TEMP_SENSOR_PCIEX16_NEAR,
        TEMP_SENSOR_ENVIRONMENT,
        TEMP_SENSOR_MEMORY_NEAR,
        TEMP_SENSOR_COUNT
};

extern const struct thermistor_info thermistor_info;

#if 0
#ifdef VARIANT_ZORK_TREMBYLE
void board_reset_pd_mcu(void);

/* Common definition for the USB PD interrupt handlers. */
void tcpc_alert_event(enum gpio_signal signal);
void bc12_interrupt(enum gpio_signal signal);
__override_proto void ppc_interrupt(enum gpio_signal signal);
#endif
#endif

void board_print_temps(void);
void apu_pcie_reset_interrupt(enum gpio_signal signal);

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */