/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Richtek RT1715 Type-C port controller */

#include "common.h"
#include "rt1715.h"
#include "tcpci.h"
#include "tcpm/tcpm.h"
#include "timer.h"
#include "usb_pd.h"

#ifndef CONFIG_USB_PD_TCPM_TCPCI
#error "RT1715 is using a standard TCPCI interface"
#error "Please upgrade your board configuration"
#endif

static int rt1715_polarity[CONFIG_USB_PD_PORT_MAX_COUNT];

static int rt1715_enable_ext_messages(int port, int enable)
{
	return tcpc_update8(port, RT1715_REG_VENDOR_5,
			    RT1715_REG_VENDOR_5_ENEXTMSG,
			    enable ? MASK_SET : MASK_CLR);
}

static int rt1715_tcpci_tcpm_init(int port)
{
	int rv;

	/* RT1715 has a vendor-defined register reset */
	rv = tcpc_update8(port, RT1715_REG_VENDOR_7,
			  RT1715_REG_VENDOR_7_SOFT_RESET, MASK_SET);
	if (rv)
		return rv;

	msleep(10);
	rv = tcpc_update8(port, RT1715_REG_VENDOR_5,
			  RT1715_REG_VENDOR_5_SHUTDOWN_OFF, MASK_SET);
	if (rv)
		return rv;

	/*
	TD.PD.LL.E2 Retransmission - Testing Downstream Port (trace)
	Checking Retransmission
	UUT must retransmit first message one time within 900 us to 1175 us (retransmitted 0 times) */
	rv = tcpc_update8(port, RT1715_REG_PHY_CTRL1,
			  RT1715_REG_PHY_CTRL1_ENRETRY, MASK_SET);
	if (rv)
		return rv;

	if (IS_ENABLED(CONFIG_USB_PD_REV30))
		rt1715_enable_ext_messages(port, 1);
    
	return tcpci_tcpm_init(port);
}

/*
 * Selects the CC PHY noise filter voltage level according to the current
 * CC voltage level.
 *
 * @param cc_level The CC voltage level for the port's current role
 * @return EC_SUCCESS if writes succeed; failure code otherwise
 */
static inline int rt1715_init_cc_params(int port, int cc_level)
{
	int rv, en, sel;

	if (cc_level == TYPEC_CC_VOLT_RP_DEF) {
		/* RXCC threshold : 0.55V */
		en = RT1715_REG_BMCIO_RXDZEN_DISABLE;

		sel = RT1715_REG_BMCIO_RXDZSEL_OCCTRL_600MA
		  | RT1715_REG_BMCIO_RXDZSEL_SEL;
	} else {
		/* RD threshold : 0.35V & RP threshold : 0.75V */
		en = RT1715_REG_BMCIO_RXDZEN_ENABLE;

		sel = RT1715_REG_BMCIO_RXDZSEL_OCCTRL_600MA;
	}

	rv = tcpc_write(port, RT1715_REG_BMCIO_RXDZEN, en);
	if (!rv)
		rv = tcpc_write(port, RT1715_REG_BMCIO_RXDZSEL, sel);

	return rv;
}

static int rt1715_get_cc(int port, enum tcpc_cc_voltage_status *cc1,
	enum tcpc_cc_voltage_status *cc2)
{
	int rv;

	rv = tcpci_tcpm_get_cc(port, cc1, cc2);
	if (rv)
		return rv;

	return rt1715_init_cc_params(port, rt1715_polarity[port] ? *cc2 : *cc1);
}

static int rt1715_set_polarity(int port, enum tcpc_cc_polarity polarity)
{
	int rv;
	enum tcpc_cc_voltage_status cc1, cc2;

	rt1715_polarity[port] = polarity;

	rv = tcpci_tcpm_get_cc(port, &cc1, &cc2);
	if (rv)
		return rv;

	rv = rt1715_init_cc_params(port, polarity ? cc2 : cc1);
	if (rv)
		return rv;

	return tcpci_tcpm_set_polarity(port, polarity);
}


const struct tcpm_drv rt1715_tcpm_drv = {
	.init = &rt1715_tcpci_tcpm_init,
	.release = &tcpci_tcpm_release,
	.get_cc = &rt1715_get_cc,
#ifdef CONFIG_USB_PD_VBUS_DETECT_TCPC
	.check_vbus_level = &tcpci_tcpm_check_vbus_level,
#endif
	.select_rp_value = &tcpci_tcpm_select_rp_value,
	.set_cc = &tcpci_tcpm_set_cc,
	.set_polarity = &rt1715_set_polarity,
#ifdef CONFIG_USB_PD_DECODE_SOP
	.sop_prime_enable	= &tcpci_tcpm_sop_prime_enable,
#endif
	.set_vconn = &tcpci_tcpm_set_vconn,
	.set_bist_test_mode = &tcpci_tcpm_set_bist_test_mode,
	.set_msg_header = &tcpci_tcpm_set_msg_header,
	.set_rx_enable = &tcpci_tcpm_set_rx_enable,
	.get_message_raw = &tcpci_tcpm_get_message_raw,
	.transmit = &tcpci_tcpm_transmit,
	.tcpc_alert = &tcpci_tcpc_alert,
#ifdef CONFIG_USB_PD_DISCHARGE_TCPC
	.tcpc_discharge_vbus = &tcpci_tcpc_discharge_vbus,
#endif
	.tcpc_enable_auto_discharge_disconnect =
		&tcpci_tcpc_enable_auto_discharge_disconnect,
#ifdef CONFIG_USB_PD_DUAL_ROLE_AUTO_TOGGLE
	.drp_toggle = &tcpci_tcpc_drp_toggle,
#endif
#ifdef CONFIG_USBC_PPC
	.set_snk_ctrl = &tcpci_tcpm_set_snk_ctrl,
	.set_src_ctrl = &tcpci_tcpm_set_src_ctrl,
#endif
	.get_chip_info = &tcpci_get_chip_info,
#ifdef CONFIG_USB_PD_TCPC_LOW_POWER
	.enter_low_power_mode = &tcpci_enter_low_power_mode,
#endif
};
