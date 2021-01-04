/*
 * Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * software watchdog for BLD.
 */
#ifndef __CROS_EC_SOFTWARE_WATCHDOG_H
#define __CROS_EC_SOFTWARE_WATCHDOG_H

typedef struct EC_POWERON_WDT {
     uint8_t wdtEn;
     uint16_t time;
     uint16_t countTime;
     uint8_t timeoutNum;
 }ec_poweron_WDT;
 
#define  POWERON_WDT_DISENABLE       0x00
#define  POWERON_WDT_ENABLE          0x01

#define  POWERON_WDT_TIMEOUT_NUM     0x02 /* */
#define  POWERON_WDT_TIMEOUT_NUM2    0x05 /* */


#endif  /* __CROS_EC_SOFTWARE_WATCHDOG_H */

