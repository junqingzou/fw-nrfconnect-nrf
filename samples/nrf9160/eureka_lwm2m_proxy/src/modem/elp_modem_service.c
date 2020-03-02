/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#include <logging/log.h>

#include <zephyr.h>
#include <stdio.h>
#include <string.h>
#include <lte_lc.h>
#include "elp_test.h"
#include "elp_connect.h"
#include "elp_modem_service.h"

#if defined(CONFIG_LTE_AUTO_INIT_AND_CONNECT)
#error CONFIG_LTE_AUTO_INIT_AND_CONNECT shoudl not be enabled
#endif

LOG_MODULE_REGISTER(modem, CONFIG_ELP_LOG_LEVEL);

static K_SEM_DEFINE(modem_connect_sem, 0, 1);

/**@brief Thread to handle LTE connection */
void lte_connect_fn(void)
{
	int err = 0;
	u8_t ret;

	while (true) {
		/* Don't go any further until connect is requested */
		k_sem_take(&modem_connect_sem, K_FOREVER);

		LOG_INF("Cellular Link Connecting...");
		err = lte_lc_init_and_connect();
		/* Not continue if connection fails*/
		__ASSERT(err == 0,
			"Cellular link not established.");
		LOG_INF("Cellular Link Connected!");

		ret = err;
		inter_connect_send(RSP_TYPE_BASE|CMD_TYPE_MDM_INT_CONNECT,
				&ret, 1);

		/* Prevent re-connect when connected */
		k_sem_reset(&modem_connect_sem);
#if defined(CONFIG_ELP_TEST)
		th_test_lte_callback(ret);
#endif
	}
}

/* size of stack area used by each thread */
#define STACKSIZE 1024
/* scheduling priority used by each thread */
#define PRIORITY 7
K_THREAD_DEFINE(lte_connect_tid, STACKSIZE, lte_connect_fn, NULL, NULL,
	NULL, PRIORITY, 0, K_NO_WAIT);

/**@brief Modem control commands */
void th_modem_control(u8_t cmd, const u8_t *param)
{
	int err = -EINVAL;

	switch (cmd) {
	case CMD_TYPE_MDM_INT_CONNECT:
		k_sem_give(&modem_connect_sem);
		break;

	case CMD_TYPE_MDM_GO_OFFLINE:
		err = lte_lc_offline();
		__ASSERT(err == 0, "Modem offline failed.");
		LOG_INF("Modem offline!");
		break;

	case CMD_TYPE_MDM_POWER_OFF:
		err = lte_lc_power_off();
		__ASSERT(err == 0, "Modem power off failed.");
		LOG_INF("Modem power off!");
		break;

	case CMD_TYPE_MDM_GO_ONLINE:
		err = lte_lc_normal();
		__ASSERT(err == 0, "Modem online failed.");
		LOG_INF("Modem online!");
		break;

	case CMD_TYPE_MDM_PSM_REQ:
		err = lte_lc_psm_req(*param);
		__ASSERT(err == 0, "Modem PSM request failed.");
		LOG_INF("Modem PSM(%d) requested!", *param);
		break;

	case CMD_TYPE_MDM_EDRX_REQ:
		err = lte_lc_edrx_req(*param);
		__ASSERT(err == 0, "Modem eDRX request failed.");
		LOG_INF("Modem eDRX(%d) requested!", *param);
		break;

	default:
		LOG_ERR("unknown %d", cmd);
		break;
	}

	if (cmd != CMD_TYPE_MDM_INT_CONNECT) {
		s8_t ret = err;
		(void)inter_connect_send(RSP_TYPE_BASE|cmd, &ret, 1);
	}
}
