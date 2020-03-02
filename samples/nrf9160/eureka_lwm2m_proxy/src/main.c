/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#include <logging/log.h>

#include <zephyr.h>
#include <stdio.h>
#include <string.h>
#include <init.h>
#include <bsd.h>
#include <lte_lc.h>
#include <modem_info.h>

#include <hal/nrf_gpio.h>
#include <hal/nrf_power.h>
#include <hal/nrf_regulators.h>

#include "elp_connect.h"
#include "elp_modem_service.h"
#include "elp_lwm2m_service.h"
#if defined(CONFIG_ELP_TEST)
#include "elp_test.h"
#endif

LOG_MODULE_REGISTER(app, CONFIG_ELP_LOG_LEVEL);

/* global variable used across different files */
struct modem_param_info modem_param;

void enter_sleep(void);

/**@brief UART RX data handling */
static void data_handler(u8_t cmd_type, const u8_t *data_buf, u8_t data_len)
{
	if (cmd_type == CMD_TYPE_DATA) {
		LOG_INF("%s", log_strdup(data_buf));
		/* Echo back data for UART testing. */
		(void)inter_connect_send(CMD_TYPE_DATA, "nRF91 ready", 11);
	}
#if defined(CONFIG_ELP_GPIO_WAKEUP)
	else if (cmd_type == (RSP_TYPE_BASE|CMD_TYPE_SYNC_CMD)) {
		LOG_INF("Sync up");
	}
	else if (cmd_type == CMD_TYPE_SLEEP_CMD) {
		th_lwm2m_control(CMD_TYPE_LWM2M_DISCONNECT, NULL, 0);
		k_sleep(1000);  //TBD
		inter_connect_uninit();
		enter_sleep();
	}
#endif
	else if ((cmd_type & 0xF0) == CMD_TYPE_MDM_BASE) {
		th_modem_control(cmd_type, data_buf);
	}
	else if ((cmd_type & 0xF0) == CMD_TYPE_LWM2M_BASE) {
		th_lwm2m_control(cmd_type, data_buf, data_len);
	}
	else {
		LOG_WRN("unknown data, dropped");
	}
}

/**@brief Recoverable BSD library error. */
void bsd_recoverable_error_handler(uint32_t err)
{
	LOG_ERR("bsdlib recoverable error: %u", err);
}

void start_execute(void)
{
	int err;

#if !defined(CONFIG_ELP_TEST)
	/* Initialize the inter connect library. */
	err = inter_connect_init(data_handler);
	if (err) {
		LOG_ERR("Init inter_connect error: %d", err);
		return;
	}
#endif /* CONFIG_ELP_TEST*/

	LOG_INF("Network service proxy starts");
	LOG_INF(" .Modem control");
	LOG_INF(" .LwM2M service");

	err = modem_info_init();
	if (err) {
		LOG_ERR("Modem info could not be established: %d", err);
		return;
	}

	modem_info_params_init(&modem_param);

#if !defined(CONFIG_ELP_TEST)
	/* Signal peer that nRF91 is ready */
	(void)inter_connect_send(CMD_TYPE_SYNC_CMD, NULL, 0);
#else
	th_test_main();
#endif	/* CONFIG_ELP_TEST */
}

#ifdef CONFIG_ELP_GPIO_WAKEUP
void enter_sleep(void)
{
	/*
	 * Due to errata 4, Always configure PIN_CNF[n].INPUT before
	 *  PIN_CNF[n].SENSE.
	 */
	nrf_gpio_cfg_input(CONFIG_ELP_MODEM_WAKEUP_PIN,
		NRF_GPIO_PIN_PULLUP);
	nrf_gpio_cfg_sense_set(CONFIG_ELP_MODEM_WAKEUP_PIN,
		NRF_GPIO_PIN_SENSE_LOW);

	/*
	 * The LTE modem also needs to be stopped by issuing a command
	 * through the modem API, before entering System OFF mode.
	 * Once the command is issued, one should wait for the modem
	 * to respond that it actually has stopped as there may be a
	 * delay until modem is disconnected from the network.
	 * Refer to https://infocenter.nordicsemi.com/topic/ps_nrf9160/
	 * pmu.html?cp=2_0_0_4_0_0_1#system_off_mode
	 */
	lte_lc_power_off();	/* Gracefully shutdown the modem.*/
	bsd_shutdown();		/* Gracefully shutdown the BSD library*/
	nrf_regulators_system_off(NRF_REGULATORS_NS);
}

void main(void)
{
	u32_t rr = nrf_power_resetreas_get(NRF_POWER_NS);

	LOG_DBG("RR: 0x%08x", rr);
	if (rr & NRF_POWER_RESETREAS_OFF_MASK) {
		nrf_power_resetreas_clear(NRF_POWER_NS, 0x70017);
		start_execute();
	} else {
		LOG_INF("Sleep");
		enter_sleep();
	}
}
#else
void main(void)
{
	start_execute();
}
#endif	/* CONFIG_ELP_GPIO_WAKEUP */
