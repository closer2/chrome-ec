/* Copyright 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "acpi.h"
#include "battery.h"
#include "common.h"
#include "console.h"
#include "dptf.h"
#include "gpio.h"
#include "hooks.h"
#include "host_command.h"
#include "keyboard_backlight.h"
#include "lpc.h"
#include "ec_commands.h"
#include "tablet_mode.h"
#include "pwm.h"
#include "timer.h"
#include "usb_charge.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_LPC, outstr)
#define CPRINTF(format, args...) cprintf(CC_LPC, format, ## args)
#define CPRINTS(format, args...) cprints(CC_LPC, format, ## args)

/* Last received ACPI command */
static uint8_t __bss_slow acpi_cmd;
/* First byte of data after ACPI command */
static uint8_t __bss_slow acpi_addr;
/* Number of data writes after command */
static int __bss_slow acpi_data_count;

#ifdef CONFIG_DPTF
static int __bss_slow dptf_temp_sensor_id;	/* last sensor ID written */
static int __bss_slow dptf_temp_threshold;	/* last threshold written */

/*
 * Current DPTF profile number.
 * This is by default initialized to 1 if multi-profile DPTF is not supported.
 * If multi-profile DPTF is supported, this is by default initialized to 2 under
 * the assumption that profile #2 corresponds to lower thresholds and is a safer
 * profile to use until board or some EC driver sets the appropriate profile for
 * device mode.
 */
static int current_dptf_profile = DPTF_PROFILE_DEFAULT;

#endif

/*
 * Keep a read cache of four bytes when burst mode is enabled, which is the
 * size of the largest non-string memmap data type.
 */
#define ACPI_READ_CACHE_SIZE 4

/* Start address that indicates read cache is flushed. */
#define ACPI_READ_CACHE_FLUSHED (EC_ACPI_MEM_MAPPED_BEGIN - 1)

/* Calculate size of valid cache based upon end of memmap data. */
#define ACPI_VALID_CACHE_SIZE(addr) (MIN( \
	EC_ACPI_MEM_MAPPED_SIZE + EC_ACPI_MEM_MAPPED_BEGIN - (addr), \
	ACPI_READ_CACHE_SIZE))

/*
 * In burst mode, read the requested memmap data and the data immediately
 * following it into a cache. For future reads in burst mode, try to grab
 * data from the cache. This ensures the continuity of multi-byte reads,
 * which is important when dealing with data types > 8 bits.
 */
static struct {
	int enabled;
	uint8_t start_addr;
	uint8_t data[ACPI_READ_CACHE_SIZE];
} acpi_read_cache;

/*
 * Deferred function to ensure that ACPI burst mode doesn't remain enabled
 * indefinitely.
 */
static void acpi_disable_burst_deferred(void)
{
	acpi_read_cache.enabled = 0;
	lpc_clear_acpi_status_mask(EC_LPC_STATUS_BURST_MODE);
	CPUTS("ACPI missed burst disable?");
}
DECLARE_DEFERRED(acpi_disable_burst_deferred);

#ifdef CONFIG_DPTF

static int acpi_dptf_is_profile_valid(int n)
{
#ifdef CONFIG_DPTF_MULTI_PROFILE
	if ((n < DPTF_PROFILE_VALID_FIRST) || (n > DPTF_PROFILE_VALID_LAST))
		return EC_ERROR_INVAL;
#else
	if (n != DPTF_PROFILE_DEFAULT)
		return EC_ERROR_INVAL;
#endif

	return EC_SUCCESS;
}

int acpi_dptf_set_profile_num(int n)
{
	int ret = acpi_dptf_is_profile_valid(n);

	if (ret == EC_SUCCESS) {
		current_dptf_profile = n;
		if (IS_ENABLED(CONFIG_DPTF_MULTI_PROFILE) &&
		    IS_ENABLED(CONFIG_HOSTCMD_EVENTS)) {
			/* Notify kernel to update DPTF profile */
			host_set_single_event(EC_HOST_EVENT_MODE_CHANGE);
		}
	}
	return ret;
}

int acpi_dptf_get_profile_num(void)
{
	return current_dptf_profile;
}

#endif

/* Read memmapped data, returns read data or 0xff on error. */
static int acpi_read(uint8_t addr)
{
    uint8_t *memmap_addr = (uint8_t *)(lpc_get_memmap_range() + addr);

    /* Read from cache if enabled (burst mode). */
    if (acpi_read_cache.enabled) {
        /* Fetch to cache on miss. */
        if (acpi_read_cache.start_addr == ACPI_READ_CACHE_FLUSHED ||
            acpi_read_cache.start_addr > addr ||
            addr - acpi_read_cache.start_addr >=
            ACPI_READ_CACHE_SIZE) {
            memcpy(acpi_read_cache.data, memmap_addr, ACPI_VALID_CACHE_SIZE(addr));
            acpi_read_cache.start_addr = addr;
        }
        
        /* Return data from cache. */
        return acpi_read_cache.data[addr - acpi_read_cache.start_addr];
    } else {
        /* Read directly from memmap data. */
        return *memmap_addr;
    }
}

static void acpi_write(uint8_t addr, int w_data)
{
    uint8_t *memmap_addr = (uint8_t *)(lpc_get_memmap_range() + addr);
    
    CPRINTS("ACPI IO(6266) write [0x%02x] -> [0x%02x] (pass)", w_data, addr);
    *memmap_addr = w_data;
}

/*
 * This handles AP writes to the EC via the ACPI I/O port. There are only a few
 * ACPI commands (EC_CMD_ACPI_*), but they are all handled here.
 */
int acpi_ap_to_ec(int is_cmd, uint8_t value, uint8_t *resultptr)
{
    int data = 0;
    int retval = 0;
    int result = 0xff;      /* value for bogus read */

    /* Read command/data; this clears the FRMH status bit. */
    if (is_cmd) {
        acpi_cmd = value;
        acpi_data_count = 0;
    } else {
        data = value;
        /*
        * The first data byte is the ACPI memory address for
        * read/write commands.
        */
        if (!acpi_data_count++)
            acpi_addr = data;
    }

    /* Process complete commands */
    if (acpi_cmd == EC_CMD_ACPI_READ && acpi_data_count == 1) {
        /* ACPI read cmd + addr */
        switch (acpi_addr) {
#ifdef CONFIG_FANS
        case EC_ACPI_MEM_FAN_DUTY:
            result = dptf_get_fan_duty_target();
            break;
#endif
#ifdef CONFIG_DPTF
        case EC_ACPI_MEM_TEMP_ID:
            result = dptf_query_next_sensor_event();
            break;
#endif

        case EC_ACPI_MEM_DEVICE_ORIENTATION:
            result = 0;
#ifdef CONFIG_DPTF
            result |= (acpi_dptf_get_profile_num() &
               EC_ACPI_MEM_DDPN_MASK)
               EC_ACPI_MEM_DDPN_SHIFT;
#endif
            break;


        default:
            result = acpi_read(acpi_addr);
            break;
        }

        /* Send the result byte */
        *resultptr = result;
        retval = 1;

    }
    else if (acpi_cmd == EC_CMD_ACPI_WRITE && acpi_data_count == 2) {
        /* ACPI write cmd + addr + data */
        switch (acpi_addr) {
#ifdef CONFIG_FANS
        case EC_ACPI_MEM_FAN_DUTY:
            dptf_set_fan_duty_target(data);
            break;
#endif

#ifdef CONFIG_DPTF
        case EC_ACPI_MEM_TEMP_ID:
            dptf_temp_sensor_id = data;
            break;
        case EC_ACPI_MEM_TEMP_THRESHOLD:
            dptf_temp_threshold = data + EC_TEMP_SENSOR_OFFSET;
            break;
        case EC_ACPI_MEM_TEMP_COMMIT:
        {
            int idx = data & EC_ACPI_MEM_TEMP_COMMIT_SELECT_MASK;
            int enable = data & EC_ACPI_MEM_TEMP_COMMIT_ENABLE_MASK;
            dptf_set_temp_threshold(dptf_temp_sensor_id,
                    dptf_temp_threshold, idx, enable);
            break;
        }
#endif

        default:
            acpi_write(acpi_addr, data);
            break;
        }
    }
    else if (acpi_cmd == EC_CMD_ACPI_QUERY_EVENT && !acpi_data_count) {
		/* Clear and return the lowest host event */
		int evt_index = lpc_get_next_host_event();
		CPRINTS("ACPI query = %d", evt_index);
		*resultptr = evt_index;
		retval = 1;
	} else if (acpi_cmd == EC_CMD_ACPI_BURST_ENABLE && !acpi_data_count) {
		/*
		 * TODO: The kernel only enables BURST when doing multi-byte
		 * value reads over the ACPI port. We don't do such reads
		 * when our memmap data can be accessed directly over LPC,
		 * so on LM4, for example, this is dead code. We might want
		 * to add a config to skip this code for certain chips.
		 */
		acpi_read_cache.enabled = 1;
		acpi_read_cache.start_addr = ACPI_READ_CACHE_FLUSHED;

		/* Enter burst mode */
		lpc_set_acpi_status_mask(EC_LPC_STATUS_BURST_MODE);

		/*
		 * Disable from deferred function in case burst mode is enabled
		 * for an extremely long time  (ex. kernel bug / crash).
		 */
		hook_call_deferred(&acpi_disable_burst_deferred_data, 1*SECOND);

		/* ACPI 5.0-12.3.3: Burst ACK */
		*resultptr = 0x90;
		retval = 1;
	} else if (acpi_cmd == EC_CMD_ACPI_BURST_DISABLE && !acpi_data_count) {
		acpi_read_cache.enabled = 0;

		/* Leave burst mode */
		hook_call_deferred(&acpi_disable_burst_deferred_data, -1);
		lpc_clear_acpi_status_mask(EC_LPC_STATUS_BURST_MODE);
	}

	return retval;
}

/*******************************************************************************
* ec host memory has 256-Byte, remap to HOST IO/900-9FF.
* we set IO/900-9CF for write protection, disable IO/9E0-9FF write protection.
*
* We defined an interface at 9E0-9FF for the BIOS to send custom commands to EC.
*
* This function is called every 10ms. BIOS must to write data firstly, and then
* write command to ec.
*
*/
static void oem_bios_to_ec_command(void)
{
    uint8_t *bios_cmd = host_get_memmap(EC_MEMMAP_BIOS_CMD);
    uint8_t *mptr = NULL;
    
    if(0 == *bios_cmd)
    {
        return;
    }

    *(bios_cmd+1) = 0x00;
    CPRINTS("BIOS command start =[0x%02x], data=[0x%02x]", *bios_cmd, *(bios_cmd+2));
    switch (*bios_cmd) {
    case 0x01 : /* BIOS write ec reset flag*/
        mptr = host_get_memmap(EC_MEMMAP_RESET_FLAG);
        *mptr = 0xAA; /* 0xAA is ec reset flag */
        *(bios_cmd+1) = 0x01; /* command status */
        break;

    case 0x02 : /* power button control */
        mptr = host_get_memmap(EC_MEMMAP_POWER_FLAG1);
        if(0x01 == *(bios_cmd+2)) /* disable */
        {
            (*mptr) |= EC_MEMMAP_POWER_LOCK;
        }
        else     /* enable */
        {
            (*mptr) &= (~EC_MEMMAP_POWER_LOCK);
        }
        *(bios_cmd+1) = 0x02;
        break;

    case 0x03 : /* system G3 control */
        mptr = host_get_memmap(EC_MEMMAP_POWER_FLAG1);
        if(0x01 == *(bios_cmd+2)) /* disable */
        {
            (*mptr) |= EC_MEMMAP_DISABLE_G3;
        }
        else     /* enable */
        {
            (*mptr) &= (~EC_MEMMAP_DISABLE_G3);
        }
        *(bios_cmd+1) = 0x03;
        break;

    default :
        break;
    }

    CPRINTS("BIOS command end =[0x%02x], data=[0x%02x]", *bios_cmd, *(bios_cmd+2));
    *bios_cmd = 0;
}
DECLARE_HOOK(HOOK_MSEC, oem_bios_to_ec_command, HOOK_PRIO_DEFAULT);


#ifdef CONFIG_BIOS_CMD_TO_EC
static int console_command_to_ec(int argc, char **argv)
{
    uint8_t *bios_cmd = host_get_memmap(EC_MEMMAP_BIOS_CMD);
    
    if (1 == argc)
    {
        return EC_ERROR_INVAL;
    }
    else
    {
        char *e;
        uint8_t d;

        if(!strcasecmp(argv[1], "reset_ctrl"))
        {
            *(bios_cmd) = 0x01;
            CPRINTS("set ec reset flasg(0xAA), ec will reset after system shutdown");
        }
        else if(!strcasecmp(argv[1], "psw_ctrl") && (3==argc))
        {
            d = strtoi(argv[2], &e, 0);
            if (*e)
                return EC_ERROR_PARAM2;

            *(bios_cmd+2) = d;
            *(bios_cmd) = 0x02;
            CPRINTS("%s power button to PCH", d?("disable"):("enable"));
        }
        else if(!strcasecmp(argv[1], "g3_ctrl") && (3==argc))
        {
            d = strtoi(argv[2], &e, 0);
            if (*e)
                return EC_ERROR_PARAM2;

            *(bios_cmd+2) = d;
            *(bios_cmd) = 0x03;
            CPRINTS("%s system G3", d?("disable"):("enable"));
        }
        else
        {
            return EC_ERROR_PARAM2;
        }
    }
    
    return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(bios_cmd, console_command_to_ec,
        "\n[reset_ctrl]\n"
        "[psw_ctrl <1/0>]\n"
        "[g3_ctrl <1/0>]\n",
        "Simulate a bios command");
#endif

