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
#define BLD_EC_VERSION_YZ       "02"
#define BLD_EC_VERSION_TEST     "00"

#define BLD_EC_VERSION_X_HEX    0x00
#define BLD_EC_VERSION_YZ_HEX   0x02
#define BLD_EC_VERSION_TEST_HEX 0x00


/*------------------------------------------------------------------------------
* NPCX7 config
------------------------------------------------------------------------------*/
#define NPCX_UART_MODULE2 1                 /* GPIO64/65 are used as UART pins. */
#define NPCX_TACH_SEL2    0                 /* 0:GPIO40/73 1:GPIO93/A6 as TACH */
#define NPCX7_PWM1_SEL    1                 /* GPIO C2 is not used as PWM1. */

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
/*#define CONFIG_CMD_AP_RESET_LOG*/         /* reset UART TX buffer*/

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

/* disable EC chip hibernate */
#undef CONFIG_HIBERNATE                     /* Enable system hibernate */

#define CONFIG_IO900_WRITE_PROTECT          /* for IO/900-9CF write protection */

#define CONFIG_CMD_POWERLED                 /* for powerled command */
#define CONFIG_BIOS_CMD_TO_EC               /* for bios command */

#define CONFIG_FLASH_LOG_OEM                /* For flash write log function*/
#define CONFIG_CMD_FLASH                    /* For flash console command function*/
#define CONFIG_CMD_RTC                      /* For RTC console command function*/
#define CONFIG_HOSTCMD_RTC                  /* For host update EC RTC*/

#define CONFIG_HOSTCMD_LPC                  /* For host command interface over LPC bus*/
#define CONFIG_UART_HOST                    /* Turn on serial port */
#define CONFIG_WMI_PORT                     /* os to ec wmi port */
#define CONFIG_MFG_MODE_FORBID_WRITE        /* MFG mode default mode, when it's no mode, forbid write mode */

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
#define CONFIG_POWER_BUTTON_LOCK_HOST       /* power button lock form host  */
#define CONFIG_POWER_BUTTON_TO_PCH_CUSTOM   /* Board provides board_pwrbtn_to_pch function*/
#define CONFIG_THROTTLE_AP                  /* Compile common code for throttling the CPU based on the temp sensors */
#define CONFIG_CPU_PROCHOT_ACTIVE_LOW       /* On x86 systems, define this option if the CPU_PROCHOT signal is active low.*/
#define CONFIG_SYSTEM_RESET_DELAY           /* EC reboot power on delay. */

/*#undef  CONFIG_EXTPOWER_DEBOUNCE_MS
#define CONFIG_EXTPOWER_DEBOUNCE_MS 200
#define CONFIG_EXTPOWER_GPIO*/
#undef CONFIG_EXTPOWER                      /* This board is desktop, NO AC status, undefine it*/

/* TODO: config fans */
#undef  CONFIG_FANS
#define CONFIG_FANS 2 
#undef CONFIG_FAN_RPM_CUSTOM
/* #define CONFIG_CUSTOM_FAN_CONTROL */
/* #ifdef VARIANT_ZORK_TREMBYLE
	#define CONFIG_FANS FAN_CH_COUNT
	#undef CONFIG_FAN_INIT_SPEED
	#define CONFIG_FAN_INIT_SPEED 50
#endif */
#define CONFIG_FAN_FAULT_CHECK_SPEED   50    /* fan check fault percent */
#define FAN_CHECK_FAULT_TIME           15    /* timebase 200ms */
#define FAN_THERMAL_CONTROL_ENABLE     30    /* timebase 200ms*/
#define FAN_DUTY_50_RPM                500   /* fan set duty 50%, check rpm > 500 */
#undef CONFIG_CONSOLE_THERMAL_TEST           /* console thermal test */
#undef CONFIG_CONSOLE_CHASSIS_TEST          /* console chassis test */

/*------------------------------------------------------------------------------
* USB-C define for pangu-l
------------------------------------------------------------------------------*/
#define CONFIG_USB
#define CONFIG_USB_PID 0x1641
#define CONFIG_USB_POWER_DELIVERY
#define CONFIG_USB_PD_TCPMV1
#define CONFIG_USB_PD_CUSTOM_PDO
#undef CONFIG_USB_PD_DUAL_ROLE
#undef CONFIG_USB_PD_INTERNAL_COMP
#define CONFIG_USB_PD_LOGGING
#undef  CONFIG_EVENT_LOG_SIZE
#define CONFIG_EVENT_LOG_SIZE 256
/* #define CONFIG_USB_PD_LOW_POWER */
#define CONFIG_USB_PD_PORT_MAX_COUNT 1
#define CONFIG_USB_PD_TCPM_TCPCI
#define CONFIG_USB_PD_VBUS_DETECT_TCPC
#define CONFIG_USB_PD_TCPC_LOW_POWER
#define CONFIG_USB_PD_TCPM_RT1715
#define CONFIG_USB_PD_REV30             /* more than zinger */
#define CONFIG_USBC_VCONN               /* more than zinger */
/* #define CONFIG_USB_PD_SIMPLE_DFP
#define CONFIG_USB_PD_VBUS_DETECT_GPIO
#define CONFIG_USBC_BACKWARDS_COMPATIBLE_DFP */

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
#define I2C_PORT_TCPC0		NPCX_I2C_PORT4_1
#define I2C_PORT_THERMAL_AP	NPCX_I2C_PORT5_1


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

/*------------------------------------------------------------------------------
* shutdown log ID define
------------------------------------------------------------------------------*/
#define LOG_ID_SHUTDOWN_0x01    0x01    /* S0, SLP_S4/S5 pull down */
#define LOG_ID_SHUTDOWN_0x02    0x02    /* S3, SLP_S4/S5 pull down */
#define LOG_ID_SHUTDOWN_0x03    0x03    /* S0, SLP_S3 pull down */
#define LOG_ID_SHUTDOWN_0x04    0x04    /* S0, SLP_S4 pull down */
#define LOG_ID_SHUTDOWN_0x05    0x05
#define LOG_ID_SHUTDOWN_0x06    0x06    /* Power button 4s timeout */
#define LOG_ID_SHUTDOWN_0x07    0x07    /* Power button 10s timeout */
#define LOG_ID_SHUTDOWN_0x08    0x08    /* S0, HWPG pull down */
#define LOG_ID_SHUTDOWN_0x09    0x09    /* wakeup WDT timeout */
#define LOG_ID_SHUTDOWN_0x0A    0x0A    /* Sx to S0, HWPG timeout WDT */
#define LOG_ID_SHUTDOWN_0x0B    0x0B    /* Sx to S0, SUSB timeout WDT */
#define LOG_ID_SHUTDOWN_0x0C    0x0C    /* Sx to S0, SUSC timeout WDT */
#define LOG_ID_SHUTDOWN_0x0D    0x0D    /* Sx to S0, SLP_S5 timeout WDT */
#define LOG_ID_SHUTDOWN_0x0E    0x0E
#define LOG_ID_SHUTDOWN_0x0F    0x0F    /* Sx to S0, RSMRST timeout WDT */
#define LOG_ID_SHUTDOWN_0x10    0x10    /* Sx to S0, PLTRST timeout WDT */
/* 11--1F reserve */

#define LOG_ID_SHUTDOWN_0x20    0x20    /* PMIC reset by voltage regulator fault */
#define LOG_ID_SHUTDOWN_0x21    0x21    /* PMIC reset by power button counter */
#define LOG_ID_SHUTDOWN_0x2E    0x2E    /* Caterr#  low trig Shutdown */

#define LOG_ID_SHUTDOWN_0x30    0x30    /* CPU too hot(internal PECI) */
#define LOG_ID_SHUTDOWN_0x31    0x31    /* CPU too hot(external NTC) */
#define LOG_ID_SHUTDOWN_0x32    0x32    /* VGA too hot */
#define LOG_ID_SHUTDOWN_0x33    0x33    /* SYS too hot(thermal sensor) */
#define LOG_ID_SHUTDOWN_0x34    0x34    /* PCH too hot(thermal sensor) */
#define LOG_ID_SHUTDOWN_0x35    0x35    /* DDR too hot(thermal sensor) */
#define LOG_ID_SHUTDOWN_0x36    0x36    /* DCJ too hot */
#define LOG_ID_SHUTDOWN_0x37    0x37    /* Ambient too hot */
#define LOG_ID_SHUTDOWN_0x38    0x38    /* SSD too hot */
#define LOG_ID_SHUTDOWN_0x39    0x39    /* battery too hot */
#define LOG_ID_SHUTDOWN_0x3A    0x3A    /* charger IC too hot */
/* 3B--3F reserve */

/* 40--4F for ODM */
#define LOG_ID_SHUTDOWN_0x40    0x40    /* Power button pressed */
#define LOG_ID_SHUTDOWN_0x41    0x41    /* Power button released */
#define LOG_ID_SHUTDOWN_0x42    0x42    /* EC reset after BIOS tool update EC */
#define LOG_ID_SHUTDOWN_0x43    0x43    /* Console command apshutdown */
#define LOG_ID_SHUTDOWN_0x44    0x44    /* shutdown wdt timeout */

/* 50--CF reserve */

#define LOG_ID_SHUTDOWN_0xD0    0xD0    /* BIOS/OS WDT time out trigger BSOD */
#define LOG_ID_SHUTDOWN_0xD1    0xD1    /* BIOS bootlock fail */
#define LOG_ID_SHUTDOWN_0xD2    0xD2    /* BIOS memory init fail */
#define LOG_ID_SHUTDOWN_0xD3    0xD3    /* BIOS main block fail */
#define LOG_ID_SHUTDOWN_0xD4    0xD4    /* BIOS crisis fail */
#define LOG_ID_SHUTDOWN_0xD5    0xD5
#define LOG_ID_SHUTDOWN_0xD6    0xD6
#define LOG_ID_SHUTDOWN_0xD7    0xD7    /* flash BIOS start */
#define LOG_ID_SHUTDOWN_0xD8    0xD8    /* flash BIOS end */
/* D9--DF reserve */

/* E0--EA reserve */
#define LOG_ID_SHUTDOWN_0xEB    0xEB    /* EC RAM init fail */
#define LOG_ID_SHUTDOWN_0xEC    0xEC    /* EC code reset */
#define LOG_ID_SHUTDOWN_0xFC    0xFC    /* EC VSTBY or WRST reset */
#define LOG_ID_SHUTDOWN_0xFD    0xFD    /* EC external WDT reset power off */
#define LOG_ID_SHUTDOWN_0xFE    0xFE    /* EC internal WDT reset power off */

/*------------------------------------------------------------------------------
* wakeup log ID define
------------------------------------------------------------------------------*/
#define LOG_ID_WAKEUP_0x01      0x01    /* power button interrupt power on */
#define LOG_ID_WAKEUP_0x02      0x02    /* power button polling power on */
#define LOG_ID_WAKEUP_0x03      0x03    /* power button wakeup S3 */
#define LOG_ID_WAKEUP_0x04      0x04    /* S3, SLP_S3 pull up */
#define LOG_ID_WAKEUP_0x05      0x05    /* S5, SLP_S4 pull up */
#define LOG_ID_WAKEUP_0x06      0x06    /* S5, SLP_S5 pull up */
/* 07--3F reserve */

/* 40--CF for ODM define */
/* ec reset cause reference <./include/reset_flag_desc.inc>*/
#define LOG_ID_WAKEUP_0x40      0x40    /* EC reset cause is reset-pin */
#define LOG_ID_WAKEUP_0x41      0x41    /* EC reset cause is power-on */
#define LOG_ID_WAKEUP_0x42      0x42    /* EC reset cause is watchdog */
#define LOG_ID_WAKEUP_0x43      0x43    /* EC reset cause is soft */
#define LOG_ID_WAKEUP_0x44      0x44    /* EC reset cause is hard */
#define LOG_ID_WAKEUP_0x45      0x45
#define LOG_ID_WAKEUP_0x46      0x46
#define LOG_ID_WAKEUP_0x47      0x47
#define LOG_ID_WAKEUP_0x48      0x48

#define LOG_ID_WAKEUP_0xD1      0xD1    /* system wakeup after BIOS update */
#define LOG_ID_WAKEUP_0xD2      0xD2    /* system wakeup from sleep */
#define LOG_ID_WAKEUP_0xD3      0xD3    /* system wakeup from hibernate */
#define LOG_ID_WAKEUP_0xD4      0xD4    /* system wakeup from power off */
/* D5--DF reserve */

/* E0--FB reserve */
#define LOG_ID_WAKEUP_0xFC      0xFC    /* EC auto power on */
#define LOG_ID_WAKEUP_0xFD      0xFD    /* system power on after mirror */
#define LOG_ID_WAKEUP_0xFE      0xFE    /* internal WDT wakeup */


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
    ADC_SENSOR_AMBIENCE_NTC = 0,
    ADC_SENSOR_SSD1_NTC,
    ADC_SENSOR_PCIEX16_NTC,
    ADC_SENSOR_CPU_NTC,
    ADC_SENSOR_MEMORY_NTC,
	ADC_CH_COUNT
};

/* TODO: need confirm with real hardware */
enum temp_sensor_id {
        TEMP_SENSOR_CPU_DTS = 0,
        TEMP_SENSOR_AMBIENCE_NTC,
        TEMP_SENSOR_SSD1_NTC,
        TEMP_SENSOR_PCIEX16_NTC,
        TEMP_SENSOR_CPU_NTC,
        TEMP_SENSOR_MEMORY_NTC,
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

void tcpc_alert_event(enum gpio_signal signal);
/* Board interfaces */
void board_set_usb_output_voltage(int mv);

#endif /* !__ASSEMBLER__ */

#endif /* __CROS_EC_BOARD_H */
