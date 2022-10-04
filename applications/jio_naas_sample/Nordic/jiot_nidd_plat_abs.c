/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <nrf_modem_at.h>
#include <modem/at_monitor.h>
#include "jiot_nidd_plat_abs.h"

LOG_MODULE_REGISTER(plat_abs, CONFIG_NAAS_LOG_LEVEL);

#define THREAD_STACK_SIZE	KB(2)
#define THREAD_PRIORITY		K_LOWEST_APPLICATION_THREAD_PRIO
#define NIDD_PAYLOAD_SIZE	1024

static struct k_thread nidd_thread;
static K_THREAD_STACK_DEFINE(nidd_thread_stack, THREAD_STACK_SIZE);
static k_tid_t nidd_thread_id;

static int nidd_sock;
static jiot_nidd_plat_event_handler event_handler;

#define EVT(evt, data, len)			\
	if (event_handler) {			\
		event_handler(evt, data, len);	\
	}

/* AT monitor for network notifications */
AT_MONITOR(network, "CEREG", cereg_mon);

static int cereg_status;
static K_SEM_DEFINE(reg_sem, 0, 1);

enum cereg_status {
	NO_NETWORK = 0,
	HOME = 1,
	SEARCHING = 2,
	DENIED = 3,
	UNKNOWN = 4,
	ROAMING = 5,
	UICC_FAILURE = 90
};

static const char *cereg_str_get(enum cereg_status status)
{
	switch (status) {
	case NO_NETWORK:
		return "no network";
	case HOME:
		return "home";
	case SEARCHING:
		return "searching";
	case DENIED:
		return "denied";
	case UNKNOWN:
		return "unknown";
	case ROAMING:
		return "roaming";
	case UICC_FAILURE:
		return "UICC failure";
	default:
		return NULL;
	}
}

static void cereg_mon(const char *notif)
{
	const char *cereg_status_str;
	cereg_status = atoi(notif + strlen("+CEREG: "));

	cereg_status_str = cereg_str_get(cereg_status);
	if (!cereg_status_str) {
		LOG_WRN("Registration status unknown: %d", cereg_status);
		return;
	}

	if (cereg_status == HOME || cereg_status == ROAMING) {
		LOG_INF("Registration status: %s", cereg_status_str);
		EVT(E_NIDD_PLAT_EVENT_CHANNEL_ACTIVATE_IND, NULL, 0);
	} else {
		LOG_DBG("Registration status: %s", cereg_status_str);
		EVT(E_NIDD_PLAT_EVENT_CHANNEL_DEACTIVATE_IND, NULL, 0);
	}

	if (cereg_status == HOME || cereg_status == ROAMING) {
		k_sem_give(&reg_sem);
	}
}

static void nidd_thread_func(void *p1, void *p2, void *p3)
{
	int ret;
	struct pollfd fds;

	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	fds.fd = nidd_sock;
	fds.events = POLLIN;
	do {
		ret = poll(&fds, 1, MSEC_PER_SEC * CONFIG_NAAS_NORDIC_NIDD_POLL_TIME);
		if (ret < 0) {  /* IO error */
			LOG_WRN("poll() error: %d", ret);
			continue;
		}
		if (ret == 0) {  /* timeout */
			continue;
		}
		LOG_DBG("Poll events 0x%08x", fds.revents);
		if ((fds.revents & POLLERR) == POLLERR) {
			LOG_WRN("POLLERR");
			break;
		}
		if ((fds.revents & POLLNVAL) == POLLNVAL) {
			LOG_WRN("POLLNVAL");
			break;
		}
		if ((fds.revents & POLLHUP) == POLLHUP) {
			/* Lose LTE connection */
			LOG_WRN("POLLHUP");
			break;
		}
		if ((fds.revents & POLLIN) != POLLIN) {
			continue;
		}
		/* Receive data */
		char rx_data[NIDD_PAYLOAD_SIZE];

		ret = recv(nidd_sock, (void *)rx_data, sizeof(rx_data), 0);
		if (ret < 0) {
			LOG_WRN("recv() error: %d", -errno);
			continue;
		}
		if (ret == 0) {
			continue;
		}
		LOG_HEXDUMP_DBG(rx_data, ret, "nidd-receive");
		EVT(E_NIDD_PLAT_EVENT_DATA_IND, rx_data, ret);
	} while (true);

	(void)close(nidd_sock);
	LOG_INF("NIDD thread terminated");
}

/*************************************************************************************************************
* @brief           Create a nidd id for specific APN.
* @param[in/out]   nidd_id -- the generated nidd id.
* @param[in]       apn     -- APN name.
* @param[in]       callback -- register the event callback to NIDD task.
* @return          jiot_plat_nidd_ret_e.
**************************************************************************************************************/
jiot_plat_nidd_ret_e jiot_nidd_plat_connect(uint32_t* nidd_id, char* apn, jiot_nidd_plat_event_handler callback)
{
	int ret;
	char cmd[128];

	LOG_DBG("id: %d, APN: %s", (int)*nidd_id, apn);
	event_handler = callback;

	ret = nrf_modem_at_printf("AT%%XSYSTEMMODE=0,1,0,0");
	if (ret) {
		LOG_ERR("Failed to set system mode: %d", ret);
		return E_NIDD_PLAT_RET_ERROR;
	}

	/* As of now, primary PDP context only */
#if defined(CONFIG_NAAS_NORDIC_SIMULATION)
	sprintf(cmd, "AT+CGDCONT=0,\"Non-IP\"");
#else
	sprintf(cmd, "AT+CGDCONT=0,\"Non-IP\",\"%s\"", apn);
#endif
	ret = nrf_modem_at_printf(cmd);
	if (ret) {
		LOG_ERR("Failed to configure PDN: %d", ret);
		return E_NIDD_PLAT_RET_ERROR;
	}
	ret = nrf_modem_at_printf("AT+CEREG=1");
	if (ret) {
		LOG_ERR("Failed to get register status: %d", ret);
		return E_NIDD_PLAT_RET_ERROR;
	}
	LOG_INF("Connecting to network, timeout in %d sec", CONFIG_NAAS_NORDIC_CONNECT_TIMEOUT);
	ret = nrf_modem_at_printf("AT+CFUN=1");
	if (ret) {
		LOG_ERR("Failed to turn on radio: %d", ret);
		return E_NIDD_PLAT_RET_ERROR;
	}
	ret = k_sem_take(&reg_sem, K_SECONDS(CONFIG_NAAS_NORDIC_CONNECT_TIMEOUT));
	if (cereg_status == HOME) {
		LOG_DBG("Network connection ready");
	} else {
		if (ret == -EAGAIN) {
			LOG_WRN("Network connection timed out");
			(void)nrf_modem_at_printf("AT+CFUN=0");
		}
		return E_NIDD_PLAT_RET_INACTIVE_ERROR;
	}

	nidd_sock = socket(AF_PACKET, SOCK_RAW, IPPROTO_IP);
	if (nidd_sock < 0) {
		LOG_ERR("socket() failed: %d", -errno);
		return E_NIDD_PLAT_RET_ERROR;
	}

	nidd_thread_id = k_thread_create(&nidd_thread, nidd_thread_stack, K_THREAD_STACK_SIZEOF(nidd_thread_stack),
			nidd_thread_func, NULL, NULL, NULL, THREAD_PRIORITY, K_USER, K_NO_WAIT);

	LOG_INF("NIDD connected");
	return E_NIDD_PLAT_RET_OK;
}

/*************************************************************************************************************
* @brief           Check whether the NIDD is activated.
* @param[in]       nidd_id -- the generated nidd id.
* @return          true: NIDD is activated, false: NIDD is not active.
**************************************************************************************************************/
bool jiot_nidd_plat_is_nidd_activated(uint32_t nidd_id)
{
	int ret;
	char pdp_type[8] = {0};

	LOG_DBG("id: %d", nidd_id);

	ret = nrf_modem_at_scanf("AT+GDCONT?",
		"+GDCONT: "
			","	/* cid */
		",\"%8\"",	/* PDP type */
		&pdp_type
	);
	if (ret < 0) {
		LOG_ERR("Failed to check PDP type: %d", ret);
		return false;
	}
	if (strcmp(pdp_type, "Non-IP")) {
		LOG_DBG("PDP type not NIDD");
		return false;
	}

	return (cereg_status == HOME || cereg_status == ROAMING);
}

/*************************************************************************************************************
* @brief           Send NIDD data to modem.
* @param[in]       nidd_id -- the 
* @param[in]       data    -- points to the data buffer..
* @param[in]       length  -- data length.
* @return          refer to jiot_plat_nidd_ret_e.
**************************************************************************************************************/
jiot_plat_nidd_ret_e jiot_nidd_plat_send_data(uint32_t nidd_id, void* data, uint16_t length)
{
	int ret;

	LOG_DBG("id: %d, len: %d", nidd_id, length);
	LOG_HEXDUMP_DBG(data, length, "nidd-send");

	ret = send(nidd_sock, data, length, 0);
	if (ret < 0) {
		LOG_ERR("send() failed: %d", -errno);
		return E_NIDD_PLAT_RET_ERROR;
	}

	return E_NIDD_PLAT_RET_OK;
}
