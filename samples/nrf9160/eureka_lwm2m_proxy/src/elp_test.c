/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#include <logging/log.h>
#include <zephyr.h>
#include <stdio.h>
#include <string.h>
#include <modem_info.h>

#include "elp_test.h"
#include "elp_connect.h"
#include "elp_modem_service.h"
#include "elp_lwm2m_service.h"

LOG_MODULE_REGISTER(test, CONFIG_ELP_LOG_LEVEL);

#if defined(CONFIG_ELP_TEST) && defined(CONFIG_ELP_GPIO_WAKEUP)
#error ELP_GPIO_WAKEUP cannot be defined when ELP_TEST is defined
#endif

static struct k_work lte_lc_work;
static K_SEM_DEFINE(lte_connected, 0, 1);

/* global variable defined in different files */
extern struct modem_param_info modem_param;

static void lte_lc_connect(struct k_work *work)
{
	ARG_UNUSED(work);
	th_modem_control(CMD_TYPE_MDM_INT_CONNECT, NULL);
}

void th_test_lte_callback(int result)
{
	if (result == 0) {
		k_sem_give(&lte_connected);
	}
}

void th_test_main(void)
{
	LOG_INF("Start testing");
	k_work_init(&lte_lc_work, lte_lc_connect);
	k_work_submit(&lte_lc_work);
	k_sem_take(&lte_connected, K_FOREVER);

	/* Test SNTP */
	/* Test LwM2M connection */
	th_lwm2m_control(CMD_TYPE_LWM2M_CONNECT, NULL, 0);
}

