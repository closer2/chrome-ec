/*
 * Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * software watchdog for BLD.
 */
#ifndef __CROS_EC_SOFTWARE_WATCHDOG_H
#define __CROS_EC_SOFTWARE_WATCHDOG_H

/* WakeUp WDT */
typedef struct EC_WAKEUP_WDT {
     uint8_t wdtEn;
     uint16_t time;
     uint16_t countTime;
     uint8_t timeoutNum;
 }ec_wakeup_WDT;

/* Shutdown WDT */
typedef struct EC_SHUTDOAN_WDT {
      uint8_t wdtEn;
      uint16_t time;
      uint16_t countTime;
      uint8_t timeoutNum;
  }ec_shutdown_WDT;

#define  SW_WDT_DISENABLE       0x00
#define  SW_WDT_ENABLE          0x01

#define  POWERON_WDT_TIMEOUT_NUM     0x02 /* */
#define  POWERON_WDT_TIMEOUT_NUM2    0x05 /* */

extern ec_wakeup_WDT g_wakeupWDT;
extern ec_shutdown_WDT g_shutdownWDT;

extern uint8_t get_chassisIntrusion_data(void);

#endif  /* __CROS_EC_SOFTWARE_WATCHDOG_H */

