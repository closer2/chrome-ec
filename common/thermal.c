/* Copyright 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* NEW thermal engine module for Chrome EC. This is a completely different
 * implementation from the original version that shipped on Link.
 */

#include "chipset.h"
#include "common.h"
#include "console.h"
#include "fan.h"
#include "hooks.h"
#include "host_command.h"
#include "temp_sensor.h"
#include "thermal.h"
#include "throttle_ap.h"
#include "timer.h"
#include "util.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_THERMAL, outstr)
#define CPRINTS(format, args...) cprints(CC_THERMAL, format, ## args)


#define UMP_SYS_FAN_START_TEMP 36 
#define UMP_CPU_FAN_START_TEMP 36 
#define GFX_SYS_FAN_START_TEMP 39 
#define GFX_CPU_FAN_START_TEMP 40 

enum thermal_mode {
    THERMAL_UMP = 0,
    THERMAL_WITH_GFX,
};

enum thermal_mode g_thermalMode;

enum thermal_fan_mode {
    UMP_THERMAL_SYS_FAN = 0,
    UMP_THERMAL_CPU_FAN,    
    GFX_THERMAL_SYS_FAN,
    GFX_THERMAL_CPU_FAN, 
};

enum thermal_level {
    LEVEL1 = 0,
    LEVEL2,
    LEVEL3,
    LEVEL4,
    LEVEL5,
    LEVEL6,
    LEVEL_COUNT
};

struct thermal_params_s {
    uint8_t   level;
    int  rpm_target;
    int   cpuCore; /* name = "CPU Core" */
    int   socNear; /* name = "SOC Near" */
    int   ssdNear; /* name = "SSD Near" */
    int   VramNear; /* name = "VRAM Near" */
    int   pcieNear; /* name = "PCIE16 Near" */
    int   ramNear; /* name = "Memory Near" */
};

struct thermal_params_s g_fanLevel[CONFIG_FANS] = {0};
struct thermal_params_s g_fanRPM[CONFIG_FANS] ={0};

uint8_t Sensorauto = 0;	/* Number of data pairs. */
int g_tempSensors[TEMP_SENSOR_COUNT] = {0};

struct thermal_level_ags {
    uint8_t    level;
    int        RPM;
    uint16_t   HowTri;
    uint16_t   lowTri;
};

struct thermal_level_s {
    const char *name;
    uint8_t num_pairs;	/* Number of data pairs. */
    const struct thermal_level_ags *data;
};

/* UMP sys fan sensor SSD near*/
const struct thermal_level_ags UMP_thermal_sys_fan_SSD_near[] = {
/* level    RPM        HowTri       lowTri */
    {0,     600,      53,   UMP_SYS_FAN_START_TEMP}, 
    {1,     800,      54,   51},  
    {2,     1000,     55,   52},  
    {3,     1300,     58,   53},
    {4,     1600,     62,   56},  
    {5,     2800,     62,   60} 
};
const struct thermal_level_s t_UMP_thermal_sys_fan_SSD_near = {
    .name = "SSD Near",
    .num_pairs = ARRAY_SIZE(UMP_thermal_sys_fan_SSD_near),
    .data = UMP_thermal_sys_fan_SSD_near,
};

/* UMP sys fan sensor memory near*/
const struct thermal_level_ags UMP_thermal_sys_fan_ram[] = {
/* level    RPM        HowTri       lowTri */
    {0,     600,      55,   UMP_SYS_FAN_START_TEMP}, 
    {1,     800,      60,   63},  
    {2,     1000,     65,   58},  
    {3,     1300,     69,   63},
    {4,     1600,     72,   67},  
    {5,     2800,     72,   70}             
};
const struct thermal_level_s t_UMP_thermal_sys_fan_ram = {
    .name = "Memory Near",
    .num_pairs = ARRAY_SIZE(UMP_thermal_sys_fan_ram),
    .data = UMP_thermal_sys_fan_ram,
};
        
/* UMP CPU fan sensor soc cor*/
const struct thermal_level_ags  UMP_thermal_cpu_soc_core[] = {
/* level    RPM        HowTri       lowTri */
    {0,     700,      60,   UMP_CPU_FAN_START_TEMP}, 
    {1,     900,      70,   57},  
    {2,     1100,     78,   67},  
    {3,     1300,     89,   75},
    {4,     1600,     96,   87},  
    {5,     2800,     96,   95}             
};
const struct thermal_level_s t_UMP_thermal_cpu_soc_core = {
    .name = "SOC Near",
    .num_pairs = ARRAY_SIZE(UMP_thermal_cpu_soc_core),
    .data = UMP_thermal_cpu_soc_core,
};
        
/* UMP CPU fan sensor VRAM*/    
const struct thermal_level_ags UMP_thermal_cpu_VRAM[] = {
/* level    RPM        HowTri       lowTri */
    {0,     700,      60,   UMP_CPU_FAN_START_TEMP}, 
    {1,     900,      68,   57},  
    {2,     1100,     75,   65},  
    {3,     1300,     82,   72},
    {4,     1600,     88,   79},  
    {5,     2800,     88,   86}             
}; 
const struct thermal_level_s t_UMP_thermal_cpu_VRAM = {
    .name = "VRAM Near",
    .num_pairs = ARRAY_SIZE(UMP_thermal_cpu_VRAM),
    .data = UMP_thermal_cpu_VRAM,
};

/*****************************************************************/
/* GFX sys fan sensor SSD near*/
const struct thermal_level_ags GFX_thermal_sys_fan_SSD_near[] = {
/* level    RPM        HowTri       lowTri */
    {0,     500,      60,   GFX_SYS_FAN_START_TEMP}, 
    {1,     600,      62,   52},  
    {2,     900,      65,   56},  
    {3,     1300,     67,   59},
    {4,     1600,     71,   61},  
    {5,     2800,     66,   64} 
};
const struct thermal_level_s t_GFX_thermal_sys_fan_SSD_near = {
    .name = "SSD Near",
    .num_pairs = ARRAY_SIZE(GFX_thermal_sys_fan_SSD_near),
    .data = GFX_thermal_sys_fan_SSD_near,
};

/* GFX sys fan sensor RAM*/  
const struct thermal_level_ags GFX_thermal_sys_fan_RAM[] = {
/* level    RPM        HowTri       lowTri */
    {0,     500,      55,   GFX_SYS_FAN_START_TEMP}, 
    {1,     600,      60,   53},  
    {2,     900,      65,   58},  
    {3,     1300,     69,   63},
    {4,     1500,     72,   67},  
    {5,     2800,     72,   70}               
};
const struct thermal_level_s t_GFX_thermal_sys_fan_RAM = {
    .name = "RAM Near",
    .num_pairs = ARRAY_SIZE(GFX_thermal_sys_fan_RAM),
    .data = GFX_thermal_sys_fan_RAM,
};

/* GFX sys fan sensor PCIEx16 */  
const struct thermal_level_ags GFX_thermal_sys_fan_PCIEx16[] = {
/* level    RPM        HowTri       lowTri */
    {0,     500,      54,   GFX_SYS_FAN_START_TEMP}, 
    {1,     600,      57,   50},  
    {2,     900,      60,   54},  
    {3,     1300,     64,   58},
    {4,     1500,     71,   62},  
    {5,     2800,     71,   69}               
};
const struct thermal_level_s t_GFX_thermal_sys_fan_PCIEx16 = {
    .name = "PCIEX16 Near",
    .num_pairs = ARRAY_SIZE(GFX_thermal_sys_fan_PCIEx16),
    .data = GFX_thermal_sys_fan_PCIEx16,
};            
/* GFX cpu fan soc_core */      
const struct thermal_level_ags GFX_thermal_cpu_fan_soc_core[] = {
/* level    RPM        HowTri       lowTri */
    {0,     800,      60,   GFX_CPU_FAN_START_TEMP}, 
    {1,     900,      70,   57},  
    {2,     1100,     78,   67},  
    {3,     1300,     89,   75},
    {4,     1600,     96,   87},  
    {5,     2800,     96,   95}               
};
const struct thermal_level_s t_GFX_thermal_cpu_fan_soc_core = {
    .name = "SOC Core",
    .num_pairs = ARRAY_SIZE(GFX_thermal_cpu_fan_soc_core),
    .data = GFX_thermal_cpu_fan_soc_core,
};
            
/* GFX cpu fan VRAM */      
const struct thermal_level_ags GFX_thermal_cpu_fan_VRAM[] = {
/* level    RPM        HowTri       lowTri */
    {0,     800,      60,   GFX_CPU_FAN_START_TEMP}, 
    {1,     900,      68,   57},  
    {2,     1100,     75,   65},  
    {3,     1300,     82,   72},
    {4,     1600,     88,   79},  
    {5,     2800,     87,   86}                   
};
const struct thermal_level_s t_GFX_thermal_cpu_fan_VRAM = {
    .name = "VRAM Near",
    .num_pairs = ARRAY_SIZE(GFX_thermal_cpu_fan_VRAM),
    .data = GFX_thermal_cpu_fan_VRAM,
};
     
int thermal_fan_percent(int low, int high, int cur)
{
	if (cur < low)
		return 0;
	if (cur > high)
		return 100;
	return 100 * (cur - low) / (high - low);
}

/* The logic below is hard-coded for only three thresholds: WARN, HIGH, HALT.
 * This is just a validity check to be sure we catch any changes in thermal.h
 */
BUILD_ASSERT(EC_TEMP_THRESH_COUNT == 3);

static uint8_t get_fan_level(uint16_t temp, uint8_t fan_level, const struct thermal_level_s *fantable)
{
	uint8_t new_level = 0x0;
    const struct thermal_level_ags *data = fantable->data;

	new_level = fan_level;
	if(fan_level < (LEVEL_COUNT - 1))
	{
		if(temp >= data[fan_level].HowTri)
		{
			new_level++;
		}
	}
	if(fan_level > 0)
	{
		if(temp < data[fan_level].lowTri)
		{
			new_level--;
		}
	}
	return new_level;

}

static uint16_t get_fan_RPM(uint8_t fan_level, const struct thermal_level_s *fantable)
{
    const struct thermal_level_ags *data = fantable->data;
    return data[fan_level].RPM;
}

#define  UMP_CPU_FAN_START (UMP_CPU_FAN_START_TEMP - 10)
#define  GFX_CPU_FAN_START (GFX_CPU_FAN_START_TEMP - 10)
static uint8_t cpu_fan_start_temp(uint8_t thermalMode)
{
    uint8_t status = 0x0;

    switch(thermalMode) { 
        case THERMAL_UMP:
            if ((g_tempSensors[TEMP_SENSOR_SOC_CORE] < UMP_CPU_FAN_START)
                && (g_tempSensors[TEMP_SENSOR_VRAM] < UMP_CPU_FAN_START)) {
                status = 0x0;
             } else {
                status = 0x55;
              }
             break;
         case THERMAL_WITH_GFX:
             if ((g_tempSensors[TEMP_SENSOR_SOC_CORE] < GFX_CPU_FAN_START)
                && (g_tempSensors[TEMP_SENSOR_VRAM] < GFX_CPU_FAN_START)) {
                status = 0x0;
             } else {
                status = 0x55;
              }
             break;
        default:
            break;
    }
    return status;
}

static uint16_t cpu_fan_check_RPM(uint8_t thermalMode)
{
    uint8_t fan = PWM_CH_CPU_FAN;
    int rpm_target = 0x0;


    /* sensor model form the configuration table board.c */
    switch(thermalMode) { 
        case THERMAL_UMP:
            /* cpu fan start status  */ 
            if (!cpu_fan_start_temp(THERMAL_UMP)) {
                rpm_target = 0x0;
                return rpm_target;
            }
            
            /* cpu fan check SOC core */      
            g_fanLevel[fan].cpuCore =
                get_fan_level(g_tempSensors[TEMP_SENSOR_SOC_CORE], 
                    g_fanLevel[fan].cpuCore, &t_UMP_thermal_cpu_soc_core);
            g_fanRPM[fan].cpuCore = get_fan_RPM(g_fanLevel[fan].cpuCore, 
                &t_UMP_thermal_cpu_soc_core);

            /* cpu fan check Vram near */    
            g_fanLevel[fan].VramNear =
                get_fan_level(g_tempSensors[TEMP_SENSOR_VRAM], 
                    g_fanLevel[fan].VramNear, &t_UMP_thermal_cpu_VRAM); 
            g_fanRPM[fan].VramNear = get_fan_RPM(g_fanLevel[fan].VramNear
                , &t_UMP_thermal_cpu_VRAM);


            rpm_target = (g_fanRPM[fan].cpuCore > g_fanRPM[fan].VramNear)
                            ?  g_fanRPM[fan].cpuCore : g_fanRPM[fan].VramNear;
            break;
        case THERMAL_WITH_GFX:
            /* cpu fan start status  */ 
            if (!cpu_fan_start_temp(THERMAL_WITH_GFX)) {
                rpm_target = 0x0;
                return rpm_target;
            }
        
            /* cpu fan check SOC core */      
            g_fanLevel[fan].cpuCore =
                get_fan_level(g_tempSensors[TEMP_SENSOR_SOC_CORE], 
                    g_fanLevel[fan].cpuCore, &t_GFX_thermal_cpu_fan_soc_core);
            g_fanRPM[fan].cpuCore = get_fan_RPM(g_fanLevel[fan].cpuCore, 
                &t_GFX_thermal_cpu_fan_soc_core);

            /* sys fan check Vram near */    
            g_fanLevel[fan].VramNear =
                get_fan_level(g_tempSensors[TEMP_SENSOR_VRAM], 
                    g_fanLevel[fan].VramNear, &t_GFX_thermal_cpu_fan_VRAM); 
            g_fanRPM[fan].VramNear = get_fan_RPM(g_fanLevel[fan].VramNear
                , &t_GFX_thermal_cpu_fan_VRAM);

            rpm_target = (g_fanRPM[fan].cpuCore > g_fanRPM[fan].VramNear)
                            ?  g_fanRPM[fan].cpuCore: g_fanRPM[fan].VramNear;
            break;  
        default:
            break;
    }
    return rpm_target;
}

#define  UMP_SYS_FAN_START (UMP_SYS_FAN_START_TEMP - 10)
#define  GFX_SYS_FAN_START (GFX_SYS_FAN_START_TEMP - 10)
static uint8_t sys_fan_start_temp(uint8_t thermalMode)
{
    uint8_t status = 0x0;

    switch(thermalMode) { 
        case THERMAL_UMP:
            if ((g_tempSensors[TEMP_SENSOR_SSD_NEAR] < UMP_SYS_FAN_START)
                && (g_tempSensors[TEMP_SENSOR_MEMORY_NEAR] < UMP_SYS_FAN_START)) {
                status = 0x0;
             } else {
                status = 0x55;
              }
             break;
         case THERMAL_WITH_GFX:
             if ((g_tempSensors[TEMP_SENSOR_SSD_NEAR] < GFX_SYS_FAN_START)
                && (g_tempSensors[TEMP_SENSOR_MEMORY_NEAR] < GFX_SYS_FAN_START)
                && (g_tempSensors[TEMP_SENSOR_PCIEX16_NEAR] < GFX_SYS_FAN_START)) {
                status = 0x0;
             } else {
                status = 0x55;
              }
             break;
        default:
            break;
    }
    return status;
}

static uint16_t sys_fan_check_RPM(uint8_t thermalMode)
{
    uint8_t fan = PWM_CH_SYS_FAN;
    int rpm_target = 0x0;

    /* sensor model form the configuration table board.c */
    switch(thermalMode) { 
        case THERMAL_UMP: 
            /* sys fan start status  */ 
            if (!sys_fan_start_temp(THERMAL_UMP)) {
                rpm_target = 0x0;
                return rpm_target;
            }

            /* sys fan check ssd near */
            g_fanLevel[fan].ssdNear =
                get_fan_level(g_tempSensors[TEMP_SENSOR_SSD_NEAR], 
                    g_fanLevel[fan].ssdNear, &t_UMP_thermal_sys_fan_SSD_near);     
            g_fanRPM[fan].ssdNear = get_fan_RPM(g_fanLevel[fan].ssdNear, 
                &t_UMP_thermal_sys_fan_SSD_near);

            /* sys fan check ram near */
            g_fanLevel[fan].ramNear =
                get_fan_level(g_tempSensors[TEMP_SENSOR_MEMORY_NEAR], 
                    g_fanLevel[fan].ramNear, &t_UMP_thermal_sys_fan_ram);  
            g_fanRPM[fan].ramNear = get_fan_RPM(g_fanLevel[fan].ramNear
                , &t_UMP_thermal_sys_fan_ram);
        
           rpm_target = (g_fanRPM[fan].ssdNear > g_fanRPM[fan].ramNear)
                            ?  g_fanRPM[fan].ssdNear : g_fanRPM[fan].ramNear;
            break;

        case THERMAL_WITH_GFX:  
            /* sys fan start status  */ 
            if (!sys_fan_start_temp(THERMAL_WITH_GFX)) {
                rpm_target = 0x0;
                return rpm_target;
            }
            
            /* sys fan check ssd near */
            g_fanLevel[fan].ssdNear =
                get_fan_level(g_tempSensors[TEMP_SENSOR_SSD_NEAR], 
                    g_fanLevel[fan].ssdNear, &t_GFX_thermal_sys_fan_SSD_near);     
            g_fanRPM[fan].ssdNear = get_fan_RPM(g_fanLevel[fan].ssdNear, 
                &t_GFX_thermal_sys_fan_SSD_near);

            /* sys fan check ram near */
            g_fanLevel[fan].ramNear =
                get_fan_level(g_tempSensors[TEMP_SENSOR_MEMORY_NEAR], 
                    g_fanLevel[fan].ramNear, &t_GFX_thermal_sys_fan_RAM);     
            g_fanRPM[fan].ramNear = get_fan_RPM(g_fanLevel[fan].ramNear, 
                &t_GFX_thermal_sys_fan_RAM);
            
            rpm_target = (g_fanRPM[fan].ssdNear > g_fanRPM[fan].ramNear)
                            ?  g_fanRPM[fan].ssdNear : g_fanRPM[fan].ramNear;
            
            /* sys fan check pcie near */
            g_fanLevel[fan].pcieNear =
                get_fan_level(g_tempSensors[TEMP_SENSOR_PCIEX16_NEAR], 
                    g_fanLevel[fan].pcieNear, &t_GFX_thermal_sys_fan_PCIEx16);     
            g_fanRPM[fan].pcieNear = get_fan_RPM(g_fanLevel[fan].pcieNear, 
                &t_GFX_thermal_sys_fan_PCIEx16);


            rpm_target = (rpm_target > g_fanRPM[fan].pcieNear)
                            ?  rpm_target : g_fanRPM[fan].pcieNear;

            break;  
        default:
            break;
    }
    return rpm_target;
}

static void thermal_control(void)
{
	uint8_t fan, rv, i;
    int tempSensors;
    int rpm_target[CONFIG_FANS] = {0x0};

	/* go through all the sensors */
	for (i = 0; i < TEMP_SENSOR_COUNT; i++) {
		/* read one */
		rv= temp_sensor_read(i, &tempSensors);

        if (rv != EC_SUCCESS)
		continue;
        tempSensors = K_TO_C(tempSensors);
        if (!Sensorauto) {
            g_tempSensors[i] = tempSensors;
        }
    }

    g_thermalMode = THERMAL_UMP;
    /* cpu thermal control */
    fan = PWM_CH_CPU_FAN;
    if (is_thermal_control_enabled(fan)) {   
        rpm_target[fan] = cpu_fan_check_RPM(g_thermalMode);
        fan_set_rpm_target(fan, rpm_target[fan]);
    }

    /* sys thermal control */
    fan = PWM_CH_SYS_FAN;
    if (is_thermal_control_enabled(fan)) {    
        rpm_target[fan] = sys_fan_check_RPM(g_thermalMode);
        fan_set_rpm_target(fan, rpm_target[fan]);
    }
}

/* Wait until after the sensors have been read */
DECLARE_HOOK(HOOK_SECOND, thermal_control, HOOK_PRIO_TEMP_SENSOR_DONE);


/*****************************************************************************/
/* Console commands */
#ifdef CONFIG_CONSOLE_THERMAL_TEST
static int sensor_count = EC_TEMP_SENSOR_ENTRIES;
static int cc_Sensorinfo(int argc, char **argv)
{
	char leader[20] = "";

    if (!Sensorauto) {
        ccprintf("%sSensorauto: YES\n", leader);  
    } else {
        ccprintf("%sSensorauto: NO\n", leader);  
    }
    
	ccprintf("%sCPU Core: %4d C\n", leader, g_tempSensors[0]);
    ccprintf("%sSOC Near: %4d C\n", leader, g_tempSensors[1]);
    ccprintf("%sSSD Near: %4d C\n", leader, g_tempSensors[2]);
    ccprintf("%sPCIEX16 Near: %4d C\n", leader, g_tempSensors[3]);        
	ccprintf("%sVRAM Near: %4d C\n", leader, g_tempSensors[4]);
    ccprintf("%sMemory Near: %4d C\n", leader, g_tempSensors[5]);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(Sensorinfo, cc_Sensorinfo,
			NULL,
			"Print Sensor info");

static int cc_sensorauto(int argc, char **argv)
{
    if (!argc) {
        Sensorauto = 0x00;
    } else {
	    Sensorauto = 0x55;
    }
    
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(sensorauto, cc_sensorauto,
			"{sensor}",
			"Enable thermal sensor control");


static int cc_sensorset(int argc, char **argv)
{
	int temp = 0;
	char *e;
	int sensor = 0;

	if (sensor_count == 0) {
		ccprintf("sensor count is zero\n");
		return EC_ERROR_INVAL;
	}

	if (sensor_count > 1) {
		if (argc < 2) {
			ccprintf("sensor number is required as the first arg\n");
			return EC_ERROR_PARAM_COUNT;
		}
		sensor = strtoi(argv[1], &e, 0);
		if (*e || sensor >= sensor_count)
			return EC_ERROR_PARAM1;
		argc--;
		argv++;
	}

	if (argc < 2)
		return EC_ERROR_PARAM_COUNT;

	temp = strtoi(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;

	ccprintf("Setting sensor %d temp to %d%%\n", sensor, temp);
    g_tempSensors[sensor] = temp;

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(sensorset, cc_sensorset,
			"{sensor} percent",
			"Set sensor temp cycle");
#endif

static int command_thermalget(int argc, char **argv)
{
	int i;

	ccprintf("sensor  warn  high  halt   fan_off fan_max   name\n");
	for (i = 0; i < TEMP_SENSOR_COUNT; i++) {
		ccprintf(" %2d      %3d   %3d    %3d    %3d     %3d     %s\n",
			 i,
			 thermal_params[i].temp_host[EC_TEMP_THRESH_WARN],
			 thermal_params[i].temp_host[EC_TEMP_THRESH_HIGH],
			 thermal_params[i].temp_host[EC_TEMP_THRESH_HALT],
			 thermal_params[i].temp_fan_off,
			 thermal_params[i].temp_fan_max,
			 temp_sensors[i].name);
	}

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(thermalget, command_thermalget,
			NULL,
			"Print thermal parameters (degrees Kelvin)");


static int command_thermalset(int argc, char **argv)
{
	unsigned int n;
	int i, val;
	char *e;

	if (argc < 3 || argc > 7)
		return EC_ERROR_PARAM_COUNT;

	n = (unsigned int)strtoi(argv[1], &e, 0);
	if (*e)
		return EC_ERROR_PARAM1;

	for (i = 2; i < argc; i++) {
		val = strtoi(argv[i], &e, 0);
		if (*e)
			return EC_ERROR_PARAM1 + i - 1;
		if (val < 0)
			continue;
		switch (i) {
		case 2:
			thermal_params[n].temp_host[EC_TEMP_THRESH_WARN] = val;
			break;
		case 3:
			thermal_params[n].temp_host[EC_TEMP_THRESH_HIGH] = val;
			break;
		case 4:
			thermal_params[n].temp_host[EC_TEMP_THRESH_HALT] = val;
			break;
		case 5:
			thermal_params[n].temp_fan_off = val;
			break;
		case 6:
			thermal_params[n].temp_fan_max = val;
			break;
		}
	}

	command_thermalget(0, 0);
	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(thermalset, command_thermalset,
			"sensor warn [high [shutdown [fan_off [fan_max]]]]",
			"Set thermal parameters (degrees Kelvin)."
			" Use -1 to skip.");

/*****************************************************************************/
/* Host commands. We'll reuse the host command number, but this is version 1,
 * not version 0. Different structs, different meanings.
 */

static enum ec_status
thermal_command_set_threshold(struct host_cmd_handler_args *args)
{
	const struct ec_params_thermal_set_threshold_v1 *p = args->params;

	if (p->sensor_num >= TEMP_SENSOR_COUNT)
		return EC_RES_INVALID_PARAM;

	thermal_params[p->sensor_num] = p->cfg;

	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_THERMAL_SET_THRESHOLD,
		     thermal_command_set_threshold,
		     EC_VER_MASK(1));

static enum ec_status
thermal_command_get_threshold(struct host_cmd_handler_args *args)
{
	const struct ec_params_thermal_get_threshold_v1 *p = args->params;
	struct ec_thermal_config *r = args->response;

	if (p->sensor_num >= TEMP_SENSOR_COUNT)
		return EC_RES_INVALID_PARAM;

	*r = thermal_params[p->sensor_num];
	args->response_size = sizeof(*r);
	return EC_RES_SUCCESS;
}
DECLARE_HOST_COMMAND(EC_CMD_THERMAL_GET_THRESHOLD,
		     thermal_command_get_threshold,
		     EC_VER_MASK(1));
