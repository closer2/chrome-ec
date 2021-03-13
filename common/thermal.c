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
#include "power.h"

/* Console output macros */
#define CPUTS(outstr) cputs(CC_THERMAL, outstr)
#define CPRINTS(format, args...) cprints(CC_THERMAL, format, ## args)

#define UMA_SYS_FAN_START_TEMP  36
#define UMA_CPU_FAN_START_TEMP  39
#define GFX_SYS_FAN_START_TEMP  39
#define GFX_CPU_FAN_START_TEMP  40

#define CPU_DTS_PROCHOT_TEMP   98
#define TEMP_MULTIPLE  100 /* TEMP_AMBIENCE_NTC */

enum thermal_mode g_thermalMode;

enum thermal_fan_mode {
    UMA_THERMAL_SYS_FAN = 0,
    UMA_THERMAL_CPU_FAN,
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
    int   rpm_target;
    int   time;
    int   cpuDts;         /* name = "CPU DTS" */
    int   ambiencerNtc;   /* name = "Ambiencer NTC" */
    int   ssd1Ntc;        /* name = "SSD1 NTC" */
    int   pcie16Ntc;      /* name = "PCIE16 NTC" */
    int   cpuNtc;         /* name = "CPU NTC" */
    int   memoryNtc;      /* name = "Memory NTC" */
    int   ssd2Ntc;        /* name = "SSD2 NTC" */
};

struct thermal_params_s g_fanLevel[CONFIG_FANS] = {0};
struct thermal_params_s g_fanRPM[CONFIG_FANS] ={0};
struct thermal_params_s g_fanProtect[TEMP_SENSOR_COUNT] ={0};

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

/* UMP sys fan sensor SSD1 NTC*/
const struct thermal_level_ags uma_thermal_sys_fan_ssd1_ntc[] = {
/* level    RPM        HowTri       lowTri */
    {0,     600,      53,   UMA_SYS_FAN_START_TEMP}, 
    {1,     800,      54,   51},  
    {2,     1000,     55,   52},  
    {3,     1300,     58,   53},
    {4,     1700,     62,   56},  
    {5,     2800,     62,   60} 
};
const struct thermal_level_s t_uma_thermal_sys_fan_ssd1_ntc = {
    .name = "SSD1 NTC",
    .num_pairs = ARRAY_SIZE(uma_thermal_sys_fan_ssd1_ntc),
    .data = uma_thermal_sys_fan_ssd1_ntc,
};

/* UMP sys fan sensor SSD2 NTC*/
const struct thermal_level_ags uma_thermal_sys_fan_ssd2_ntc[] = {
/* level    RPM        HowTri       lowTri */
    {0,     600,      64,   UMA_SYS_FAN_START_TEMP}, 
    {1,     800,      65,   62},  
    {2,     1000,     66,   63},  
    {3,     1300,     72,   64},
    {4,     1700,     78,   69},  
    {5,     2800,     78,   76} 
};
const struct thermal_level_s t_uma_thermal_sys_fan_ssd2_ntc = {
    .name = "SSD2 NTC",
    .num_pairs = ARRAY_SIZE(uma_thermal_sys_fan_ssd2_ntc),
    .data = uma_thermal_sys_fan_ssd2_ntc,
};

/* UMP sys fan sensor memory NTC*/
const struct thermal_level_ags uma_thermal_sys_fan_memory_ntc[] = {
/* level    RPM        HowTri       lowTri */
    {0,     600,      55,   UMA_SYS_FAN_START_TEMP}, 
    {1,     800,      60,   53},  
    {2,     1000,     65,   58},  
    {3,     1300,     69,   63},
    {4,     1700,     72,   67},  
    {5,     2800,     72,   70}
};
const struct thermal_level_s t_uma_thermal_sys_fan_memory_ntc = {
    .name = "Memory NTC",
    .num_pairs = ARRAY_SIZE(uma_thermal_sys_fan_memory_ntc),
    .data = uma_thermal_sys_fan_memory_ntc,
};
        
/* UMP CPU fan sensor CPU DTS*/
const struct thermal_level_ags  uma_thermal_cpu_fan_cpu_dts[] = {
/* level    RPM        HowTri       lowTri */
    {0,     700,      60,   UMA_CPU_FAN_START_TEMP}, 
    {1,     900,      70,   57},  
    {2,     1100,     78,   67},  
    {3,     1300,     89,   75},
    {4,     1700,     96,   85},  
    {5,     2800,     96,   95}
};
const struct thermal_level_s t_uma_thermal_cpu_fan_cpu_dts = {
    .name = "CPU DTS",
    .num_pairs = ARRAY_SIZE(uma_thermal_cpu_fan_cpu_dts),
    .data = uma_thermal_cpu_fan_cpu_dts,
};
        
/* UMP CPU fan sensor CPU NTC*/    
const struct thermal_level_ags uma_thermal_cpu_fan_cpu_ntc[] = {
/* level    RPM        HowTri       lowTri */
    {0,     700,      60,   UMA_CPU_FAN_START_TEMP}, 
    {1,     900,      68,   57},  
    {2,     1100,     75,   65},  
    {3,     1300,     82,   72},
    {4,     1700,     88,   79},  
    {5,     2800,     88,   86}             
}; 
const struct thermal_level_s t_uma_thermal_cpu_fan_cpu_ntc = {
    .name = "CPU NTC",
    .num_pairs = ARRAY_SIZE(uma_thermal_cpu_fan_cpu_ntc),
    .data = uma_thermal_cpu_fan_cpu_ntc,
};

/*****************************************************************/
/* GFX sys fan sensor SSD1 NTC*/
const struct thermal_level_ags gfx_thermal_sys_fan_ssd1_ntc[] = {
/* level    RPM        HowTri       lowTri */
    {0,     500,      60,   GFX_SYS_FAN_START_TEMP}, 
    {1,     600,      62,   52},  
    {2,     900,      65,   56},  
    {3,     1300,     67,   59},
    {4,     1600,     71,   61},  
    {5,     2800,     66,   64} 
};
const struct thermal_level_s t_gfx_thermal_sys_fan_ssd1_ntc = {
    .name = "SSD1 NTC",
    .num_pairs = ARRAY_SIZE(gfx_thermal_sys_fan_ssd1_ntc),
    .data = gfx_thermal_sys_fan_ssd1_ntc,
};

/* GFX sys fan sensor MEMORY NTC*/  
const struct thermal_level_ags gfx_thermal_sys_fan_memory_ntc[] = {
/* level    RPM        HowTri       lowTri */
    {0,     500,      55,   GFX_SYS_FAN_START_TEMP}, 
    {1,     600,      60,   53},  
    {2,     900,      65,   58},  
    {3,     1300,     69,   63},
    {4,     1500,     72,   67},  
    {5,     2800,     72,   70}               
};
const struct thermal_level_s t_gfx_thermal_sys_fan_memory_ntc = {
    .name = "Memory NTC",
    .num_pairs = ARRAY_SIZE(gfx_thermal_sys_fan_memory_ntc),
    .data = gfx_thermal_sys_fan_memory_ntc,
};

/* GFX sys fan sensor PCIEx16 NTC */  
const struct thermal_level_ags gfx_thermal_sys_fan_pciex16_ntc[] = {
/* level    RPM        HowTri       lowTri */
    {0,     500,      54,   GFX_SYS_FAN_START_TEMP}, 
    {1,     600,      57,   50},  
    {2,     900,      60,   54},  
    {3,     1300,     64,   58},
    {4,     1500,     71,   62},  
    {5,     2800,     71,   69}               
};
const struct thermal_level_s t_gfx_thermal_sys_fan_pciex16_ntc = {
    .name = "PCIEX16 NTC",
    .num_pairs = ARRAY_SIZE(gfx_thermal_sys_fan_pciex16_ntc),
    .data = gfx_thermal_sys_fan_pciex16_ntc,
};            
/* GFX cpu fan CPU DTS */      
const struct thermal_level_ags gfx_thermal_cpu_fan_cpu_dts[] = {
/* level    RPM        HowTri       lowTri */
    {0,     800,      60,   GFX_CPU_FAN_START_TEMP}, 
    {1,     900,      70,   57},  
    {2,     1100,     78,   67},  
    {3,     1300,     89,   75},
    {4,     1600,     96,   87},  
    {5,     2800,     96,   95}               
};
const struct thermal_level_s t_gfx_thermal_cpu_fan_cpu_dts = {
    .name = "CPU DTS",
    .num_pairs = ARRAY_SIZE(gfx_thermal_cpu_fan_cpu_dts),
    .data = gfx_thermal_cpu_fan_cpu_dts,
};
            
/* GFX cpu fan CPU NTC */      
const struct thermal_level_ags gfx_thermal_cpu_fan_cpu_ntc[] = {
/* level    RPM        HowTri       lowTri */
    {0,     800,      60,   GFX_CPU_FAN_START_TEMP}, 
    {1,     900,      68,   57},  
    {2,     1100,     75,   65},  
    {3,     1300,     82,   72},
    {4,     1600,     88,   79},  
    {5,     2800,     87,   86}                   
};
const struct thermal_level_s t_gfx_thermal_cpu_fan_cpu_ntc = {
    .name = "CPU NTC ",
    .num_pairs = ARRAY_SIZE(gfx_thermal_cpu_fan_cpu_ntc),
    .data = gfx_thermal_cpu_fan_cpu_ntc,
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

void thermal_type(enum thermal_mode type)
{
    if (type > THERMAL_WITH_GFX) {
        return;
    }

    g_thermalMode = type;
}

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

static int get_fan_RPM(uint8_t fan_level, const struct thermal_level_s *fantable)
{
    const struct thermal_level_ags *data = fantable->data;
    return data[fan_level].RPM;
}

static int cpu_fan_start_temp(uint8_t thermalMode)
{
    int temp = 0x0;

    switch(thermalMode) {
        case THERMAL_UMA:
            if (g_tempSensors[TEMP_SENSOR_AMBIENCE_NTC] >= UMA_CPU_FAN_START_TEMP) {
                temp = (g_tempSensors[TEMP_SENSOR_AMBIENCE_NTC]
                    - UMA_CPU_FAN_START_TEMP) * TEMP_MULTIPLE;
            }
            break;
         case THERMAL_WITH_GFX:
            if (g_tempSensors[TEMP_SENSOR_AMBIENCE_NTC] >= GFX_CPU_FAN_START_TEMP) {
                temp = (g_tempSensors[TEMP_SENSOR_AMBIENCE_NTC]
                    - GFX_CPU_FAN_START_TEMP) * TEMP_MULTIPLE;
            }
            break;
        default:
            break;
    }
    return temp;
}

static int cpu_fan_check_RPM(uint8_t thermalMode)
{
    uint8_t fan = PWM_CH_CPU_FAN;
    int rpm_target = 0x0;
    int temp = 0x0;

    /* sensor model form the configuration table board.c */
    switch(thermalMode) { 
        case THERMAL_UMA:
            /* cpu fan start status  */ 
            temp = cpu_fan_start_temp(THERMAL_UMA);

            /* cpu fan check CPU DTS */
            g_fanLevel[fan].cpuDts =
                get_fan_level(g_tempSensors[TEMP_SENSOR_CPU_DTS], 
                    g_fanLevel[fan].cpuDts, &t_uma_thermal_cpu_fan_cpu_dts);
            g_fanRPM[fan].cpuDts = get_fan_RPM(g_fanLevel[fan].cpuDts, 
                &t_uma_thermal_cpu_fan_cpu_dts);

            /* cpu fan check CPU NTC */    
            g_fanLevel[fan].cpuNtc =
                get_fan_level(g_tempSensors[TEMP_SENSOR_CPU_NTC], 
                    g_fanLevel[fan].cpuNtc, &t_uma_thermal_cpu_fan_cpu_ntc); 
            g_fanRPM[fan].cpuNtc = get_fan_RPM(g_fanLevel[fan].cpuNtc
                , &t_uma_thermal_cpu_fan_cpu_ntc);


            rpm_target = (g_fanRPM[fan].cpuDts > g_fanRPM[fan].cpuNtc)
                            ?  g_fanRPM[fan].cpuDts : g_fanRPM[fan].cpuNtc;
            rpm_target += temp;
            break;
        case THERMAL_WITH_GFX:
            /* cpu fan start status  */ 
            temp = cpu_fan_start_temp(THERMAL_WITH_GFX);

            /* cpu fan check CPU DTS */      
            g_fanLevel[fan].cpuDts =
                get_fan_level(g_tempSensors[TEMP_SENSOR_CPU_DTS], 
                    g_fanLevel[fan].cpuDts, &t_gfx_thermal_cpu_fan_cpu_dts);
            g_fanRPM[fan].cpuDts = get_fan_RPM(g_fanLevel[fan].cpuDts, 
                &t_gfx_thermal_cpu_fan_cpu_dts);

            /* sys fan check CPU NTC */    
            g_fanLevel[fan].cpuNtc =
                get_fan_level(g_tempSensors[TEMP_SENSOR_CPU_NTC], 
                    g_fanLevel[fan].cpuNtc, &t_gfx_thermal_cpu_fan_cpu_ntc); 
            g_fanRPM[fan].cpuNtc = get_fan_RPM(g_fanLevel[fan].cpuNtc
                , &t_gfx_thermal_cpu_fan_cpu_ntc);

            rpm_target = (g_fanRPM[fan].cpuDts > g_fanRPM[fan].cpuNtc)
                            ?  g_fanRPM[fan].cpuDts: g_fanRPM[fan].cpuNtc;
            rpm_target += temp;
            break;  
        default:
            break;
    }
    return rpm_target;
}

/* ambience NTC */
static int sys_fan_start_temp(uint8_t thermalMode)
{
    int temp = 0x0;

    switch(thermalMode) { 
        case THERMAL_UMA:
            if (g_tempSensors[TEMP_SENSOR_AMBIENCE_NTC] >= UMA_SYS_FAN_START_TEMP) {
                temp = (g_tempSensors[TEMP_SENSOR_AMBIENCE_NTC] 
                    - UMA_SYS_FAN_START_TEMP) * TEMP_MULTIPLE;
            }
            break;
         case THERMAL_WITH_GFX:
             if (g_tempSensors[TEMP_SENSOR_AMBIENCE_NTC] >= GFX_SYS_FAN_START_TEMP) {
                 temp = (g_tempSensors[TEMP_SENSOR_AMBIENCE_NTC] 
                     - GFX_SYS_FAN_START_TEMP) * TEMP_MULTIPLE;
             }
             break;
        default:
            break;
    }
    return temp;
}

static int sys_fan_check_RPM(uint8_t thermalMode)
{
    uint8_t fan = PWM_CH_SYS_FAN;
    int rpm_target = 0x0;
    int temp = 0x0;

    /* sensor model form the configuration table board.c */
    switch(thermalMode) {
        case THERMAL_UMA:
            /* sys fan start status ambience NTC */
            temp = sys_fan_start_temp(THERMAL_UMA);

            /* sys fan check SSD1 NTC */
            g_fanLevel[fan].ssd1Ntc =
                get_fan_level(g_tempSensors[TEMP_SENSOR_SSD1_NTC], 
                    g_fanLevel[fan].ssd1Ntc, &t_uma_thermal_sys_fan_ssd1_ntc);
            g_fanRPM[fan].ssd1Ntc = get_fan_RPM(g_fanLevel[fan].ssd1Ntc, 
                &t_uma_thermal_sys_fan_ssd1_ntc);

            /* sys fan check SSD2 NTC */
            g_fanLevel[fan].ssd2Ntc =
                get_fan_level(g_tempSensors[TEMP_SENSOR_SSD2_NTC], 
                g_fanLevel[fan].ssd2Ntc, &t_uma_thermal_sys_fan_ssd2_ntc);
            g_fanRPM[fan].ssd2Ntc = get_fan_RPM(g_fanLevel[fan].ssd2Ntc, 
                &t_uma_thermal_sys_fan_ssd2_ntc);

            rpm_target = (g_fanRPM[fan].ssd1Ntc > g_fanRPM[fan].ssd2Ntc)
                            ?  g_fanRPM[fan].ssd1Ntc : g_fanRPM[fan].ssd2Ntc;

            /* sys fan check Memory NTC */
            g_fanLevel[fan].memoryNtc =
                get_fan_level(g_tempSensors[TEMP_SENSOR_MEMORY_NTC],
                    g_fanLevel[fan].memoryNtc, &t_uma_thermal_sys_fan_memory_ntc);
            g_fanRPM[fan].memoryNtc = get_fan_RPM(g_fanLevel[fan].memoryNtc
                , &t_uma_thermal_sys_fan_memory_ntc);

            rpm_target = (rpm_target > g_fanRPM[fan].memoryNtc)
                            ?  rpm_target : g_fanRPM[fan].memoryNtc;
            rpm_target += temp;
            break;

        case THERMAL_WITH_GFX:
            /* sys fan start status ambience NTC */
            temp = sys_fan_start_temp(THERMAL_WITH_GFX);

            /* sys fan check SSD1 NTC */
            g_fanLevel[fan].ssd1Ntc =
                get_fan_level(g_tempSensors[TEMP_SENSOR_SSD1_NTC], 
                    g_fanLevel[fan].ssd1Ntc, &t_gfx_thermal_sys_fan_ssd1_ntc);
            g_fanRPM[fan].ssd1Ntc = get_fan_RPM(g_fanLevel[fan].ssd1Ntc,
                &t_gfx_thermal_sys_fan_ssd1_ntc);

            /* sys fan check Memory NTC */
            g_fanLevel[fan].memoryNtc =
                get_fan_level(g_tempSensors[TEMP_SENSOR_MEMORY_NTC], 
                    g_fanLevel[fan].memoryNtc, &t_gfx_thermal_sys_fan_memory_ntc);
            g_fanRPM[fan].memoryNtc = get_fan_RPM(g_fanLevel[fan].memoryNtc, 
                &t_gfx_thermal_sys_fan_memory_ntc);
            
            rpm_target = (g_fanRPM[fan].ssd1Ntc > g_fanRPM[fan].memoryNtc)
                            ?  g_fanRPM[fan].ssd1Ntc : g_fanRPM[fan].memoryNtc;

            /* sys fan check pciE16 NTC */
            g_fanLevel[fan].pcie16Ntc =
                get_fan_level(g_tempSensors[TEMP_SENSOR_PCIEX16_NTC], 
                    g_fanLevel[fan].pcie16Ntc, &t_gfx_thermal_sys_fan_pciex16_ntc);
            g_fanRPM[fan].pcie16Ntc = get_fan_RPM(g_fanLevel[fan].pcie16Ntc, 
                &t_gfx_thermal_sys_fan_pciex16_ntc);


            rpm_target = (rpm_target > g_fanRPM[fan].pcie16Ntc)
                            ?  rpm_target : g_fanRPM[fan].pcie16Ntc;
            rpm_target += temp;
            break;
        default:
            break;
    }
    return rpm_target;
}

/* Device high temperature protection mechanism */
#define TEMP_CPU_DTS_PROTECTION        105
#define TEMP_CPU_NTC_PROTECTION        105
#define TEMP_SSD1_NTC_PROTECTION        90
#define TEMP_SSD2_NTC_PROTECTION        90
#define TEMP_MEMORY_NTC_PROTECTION      90
#define TEMP_AMBIENT_NTC_PROTECTION     70

#define TEMP_PROTECTION_COUNT 5
static void temperature_protection_mechanism(void)
{
    #if 0
    /* Device high temperature protection mechanism */
    if (g_tempSensors[TEMP_SENSOR_CPU_DTS] > CPU_DTS_PROCHOT_TEMP) {
        gpio_set_level(GPIO_PROCHOT_ODL, 0); /* low Prochot enable */
    } else if (g_tempSensors[TEMP_SENSOR_CPU_DTS] < CPU_DTS_PROCHOT_TEMP - 6) {
        gpio_set_level(GPIO_PROCHOT_ODL, 1); /* high Prochot disable */
    }
    #endif
    /* CPU DTS */
    if (g_tempSensors[TEMP_SENSOR_CPU_DTS] > TEMP_CPU_DTS_PROTECTION) {
        g_fanProtect[TEMP_SENSOR_CPU_DTS].time++;
    } else {
        if (g_fanProtect[TEMP_SENSOR_CPU_DTS].time > 0) {
            g_fanProtect[TEMP_SENSOR_CPU_DTS].time--;
        }
    }
    if (g_fanProtect[TEMP_SENSOR_CPU_DTS].time > TEMP_PROTECTION_COUNT) {
        chipset_force_shutdown(LOG_ID_SHUTDOWN_0x30);
        g_fanProtect[TEMP_SENSOR_CPU_DTS].time = 0;
    }

    /* CPU NTC */
    if (g_tempSensors[TEMP_SENSOR_CPU_NTC] > TEMP_CPU_NTC_PROTECTION) {
        g_fanProtect[TEMP_SENSOR_CPU_NTC].time++;
    } else {
        if (g_fanProtect[TEMP_SENSOR_CPU_NTC].time > 0) {
            g_fanProtect[TEMP_SENSOR_CPU_NTC].time--;
        }
    }
    if (g_fanProtect[TEMP_SENSOR_CPU_NTC].time > TEMP_PROTECTION_COUNT) {
        chipset_force_shutdown(LOG_ID_SHUTDOWN_0x31);
        g_fanProtect[TEMP_SENSOR_CPU_NTC].time = 0;
    }

    /* SSD NTC */
    if (g_tempSensors[TEMP_SENSOR_SSD1_NTC] > TEMP_SSD1_NTC_PROTECTION) {
        g_fanProtect[TEMP_SENSOR_SSD1_NTC].time++;
    } else {
        if (g_fanProtect[TEMP_SENSOR_SSD1_NTC].time > 0) {
            g_fanProtect[TEMP_SENSOR_SSD1_NTC].time--;
        }
    }
    if (g_fanProtect[TEMP_SENSOR_SSD1_NTC].time > TEMP_PROTECTION_COUNT) {
        chipset_force_shutdown(LOG_ID_SHUTDOWN_0x38);
        g_fanProtect[TEMP_SENSOR_SSD1_NTC].time = 0;
    }

    /* memory NTC */
    if (g_tempSensors[TEMP_SENSOR_MEMORY_NTC] > TEMP_MEMORY_NTC_PROTECTION) {
        g_fanProtect[TEMP_SENSOR_MEMORY_NTC].time++;
    } else {
        if (g_fanProtect[TEMP_SENSOR_MEMORY_NTC].time > 0) {
            g_fanProtect[TEMP_SENSOR_MEMORY_NTC].time--;
        }
    }
    if (g_fanProtect[TEMP_SENSOR_MEMORY_NTC].time > TEMP_PROTECTION_COUNT) {
        chipset_force_shutdown(LOG_ID_SHUTDOWN_0x35);
        g_fanProtect[TEMP_SENSOR_MEMORY_NTC].time = 0;
    }

    /* ambience NTC */
    if (g_tempSensors[TEMP_SENSOR_AMBIENCE_NTC] > TEMP_AMBIENT_NTC_PROTECTION) {
        g_fanProtect[TEMP_SENSOR_AMBIENCE_NTC].time++;
    } else {
        if (g_fanProtect[TEMP_SENSOR_AMBIENCE_NTC].time > 0) {
            g_fanProtect[TEMP_SENSOR_AMBIENCE_NTC].time--;
        }
    }
    if (g_fanProtect[TEMP_SENSOR_AMBIENCE_NTC].time > TEMP_PROTECTION_COUNT) {
        chipset_force_shutdown(LOG_ID_SHUTDOWN_0x37);
        g_fanProtect[TEMP_SENSOR_AMBIENCE_NTC].time = 0;
    }

    /* SSD2 NTC */
    if (g_tempSensors[TEMP_SENSOR_SSD2_NTC] > TEMP_SSD2_NTC_PROTECTION) {
        g_fanProtect[TEMP_SENSOR_SSD2_NTC].time++;
    } else {
        if (g_fanProtect[TEMP_SENSOR_SSD2_NTC].time > 0) {
            g_fanProtect[TEMP_SENSOR_SSD2_NTC].time--;
        }
    }
    if (g_fanProtect[TEMP_SENSOR_SSD2_NTC].time > TEMP_PROTECTION_COUNT) {
        chipset_force_shutdown(LOG_ID_SHUTDOWN_0x49);
        g_fanProtect[TEMP_SENSOR_SSD2_NTC].time = 0;
    }

}

static void thermal_control(void)
{
    uint8_t fan, i;
    int tempSensors;
    int rpm_target[CONFIG_FANS] = {0x0};
    uint8_t *mptr = NULL;

    /* go through all the sensors */
    mptr = (uint8_t *)host_get_memmap(EC_MEMMAP_TEMP_SENSOR_AVG);
    for (i = 0; i < TEMP_SENSOR_COUNT; i++) {
        /* read one */

        tempSensors = *(mptr + i);
        if (!Sensorauto) {
            g_tempSensors[i] = tempSensors;
        }
    }

    /* Device high temperature protection mechanism */
     mptr = (uint8_t *)host_get_memmap(EC_MEMMAP_SYS_MISC1);
    if (*mptr & EC_MEMMAP_ACPI_MODE) {
        temperature_protection_mechanism();
    }

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
DECLARE_HOOK(HOOK_SECOND, thermal_control, HOOK_PRIO_TEMP_SENSOR_DONE + 1);


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
    
    ccprintf("%sCPU DTS: %4d C\n", leader, g_tempSensors[0]);
    ccprintf("%sAmbiencer NTC: %4d C\n", leader, g_tempSensors[1]);
    ccprintf("%sSSD1 NTC: %4d C\n", leader, g_tempSensors[2]);
    ccprintf("%sPCIE16 NTC: %4d C\n", leader, g_tempSensors[3]);
	ccprintf("%sCPU NTC: %4d C\n", leader, g_tempSensors[4]);
    ccprintf("%sMemory NTC: %4d C\n", leader, g_tempSensors[5]);
    ccprintf("%sSSD2 NTC: %4d C\n", leader, g_tempSensors[6]);

	return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(Sensorinfo, cc_Sensorinfo,
			NULL,
			"Print Sensor info");

static int cc_sensorauto(int argc, char **argv)
{
    char *e;
    int input = 0;

    if (sensor_count == 0) {
        ccprintf("sensor count is zero\n");
        return EC_ERROR_INVAL;
    }

    if (argc < 2) {
        ccprintf("fan number is required as the first arg\n");
        return EC_ERROR_PARAM_COUNT;
    }
    input = strtoi(argv[1], &e, 0);
    if (*e || input > 0x01)
        return EC_ERROR_PARAM1;
    argc--;
    argv++;

    if (!input) {
        Sensorauto = 0x00;
    } else {
        Sensorauto = 0x55;
    }
    return EC_SUCCESS;
}
DECLARE_CONSOLE_COMMAND(sensorauto, cc_sensorauto,
            "{0:auto enable 1:auto disable}",
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
