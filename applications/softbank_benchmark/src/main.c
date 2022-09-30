/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <nrf_modem_at.h>
#include <modem/nrf_modem_lib.h>
#include <modem/at_monitor.h>
#include <modem/modem_info.h>

LOG_MODULE_REGISTER(demo, 3);

#define SBM_M1_DEMO	0
#define SBM_NB1_DEMO	1

static uint64_t tm_start;
static uint64_t rrc_connect_time;
static bool rai_enabled;

static uint64_t metric_attach_time;
static uint64_t metric_inactivity_time;
static uint64_t metric_udp_dns_time;
static uint64_t metric_udp_rai_time;
static uint64_t metric_tcp_connect_time;
static uint64_t metric_tcp_txrx_time;

/* Semaphores */
static K_SEM_DEFINE(rrc_sem, 0, 1);
static K_SEM_DEFINE(sleep_sem, 0, 1);
static K_SEM_DEFINE(demo_sem, 0, 1);

static struct k_work_delayable udp_work;
static struct k_work_delayable tcp_work;

/* AT monitor for network notifications */
AT_MONITOR(network, "CEREG", cereg_mon);
/* Monitors are enabled by default, but an initial state may be set optionally.
 * AT monitor for link quality notifications, paused.
 */
AT_MONITOR(link_quality, "CESQ", cesq_mon);
/* AT monitor for RRC connection */
AT_MONITOR(rrc_evt, "CSCON", rrc_mon);
/* AT monitor for modem event */
AT_MONITOR(mdm_evt, "MDMEV", mdmevt_mon);
/* AT monitor for eDRX/PSM sleep */
AT_MONITOR(sleep_evt, "XMODEMSLEEP", sleep_mon, PAUSED);
/* AT monitor for periodic TAU */
AT_MONITOR(tau_evt, "XT3412", tau_mon, PAUSED);

static int cereg_status;
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

static int rsrp_status;
static int rrc_status;

static void cereg_mon(const char *notif)
{
	const char *cereg_status_str;
	static int cereg_status_bak = NO_NETWORK;

	cereg_status = atoi(notif + strlen("+CEREG: "));
	if (cereg_status == SEARCHING && cereg_status_bak != NO_NETWORK) {
		/* For re-register */
		tm_start = k_uptime_get();
	}

	cereg_status_str = cereg_str_get(cereg_status);
	if (!cereg_status_str) {
		LOG_INF("Registration status unknown: %d", cereg_status);
		return;
	}

	if (cereg_status == HOME || cereg_status == ROAMING) {
		metric_attach_time = k_uptime_delta(&tm_start);
		LOG_INF("Registration status: %s, used %d.%03d sec", cereg_status_str,
			(int)(metric_attach_time/1000), (int)(metric_attach_time%1000));
		at_monitor_pause(&link_quality);
		at_monitor_pause(&mdm_evt);
		at_monitor_resume(&sleep_evt);
		at_monitor_resume(&tau_evt);
	} else {
		LOG_INF("Registration status: %s", cereg_status_str);
	}

	if (cereg_status == HOME) {
		k_sem_give(&demo_sem);
	}
	cereg_status_bak = cereg_status;
}

static void cesq_mon(const char *notif)
{
	rsrp_status = atoi(notif + strlen("%CESQ: "));

	LOG_INF("Link quality: %d dBm", RSRP_IDX_TO_DBM(rsrp_status));
}

static void rrc_mon(const char *notif)
{
	static uint64_t uptime;

	rrc_status = atoi(notif + strlen("+CSCON: "));

	if (rrc_status) {
		uptime = k_uptime_get();
		LOG_INF(">>> RRC connected");
		k_sem_reset(&rrc_sem);
	} else {
		uint64_t connect_delta = k_uptime_delta(&uptime);
		LOG_INF("<<< RRC released in %d.%03d sec", (int)(connect_delta/1000),
			(int)(connect_delta%1000));
		rrc_connect_time += connect_delta;
		if (rai_enabled) {
			metric_udp_rai_time = connect_delta;
			rai_enabled = false;
		}
		k_sem_give(&rrc_sem);
	}
}

static void mdmevt_mon(const char *notif)
{
	LOG_INF("%s", notif);
}

static void sleep_mon(const char *notif)
{
	int time;

	if (NULL != strstr(notif, "%XMODEMSLEEP: 2")) {
		time = atoi(notif + strlen("%XMODEMSLEEP: 2,"));
		LOG_INF("eDRX - %s", notif);
		if (time > 0) {
			k_sem_give(&sleep_sem);
		}
	} else if (NULL != strstr(notif, "%XMODEMSLEEP: 1")) {
		time = atoi(notif + strlen("%XMODEMSLEEP: 1,"));
		LOG_INF("PSM - %s", notif);
		if (time > 0) {
			k_sem_give(&sleep_sem);
		}
	} else {
		LOG_INF("%s", notif);
	}
}

static void tau_mon(const char *notif)
{
	LOG_INF("PSM - %s", notif);
}

static void psm_read(void)
{
	int ret;
	int psm_enabled;
	char request_periodic_tau[8];
	char request_active_time[8];

	LOG_INF("Reading PSM info...");
	ret = nrf_modem_at_scanf("AT+CPSMS?",
		"+CPSMS: "
			"%d"	/* enabled */
			","	/* Requested_Periodic-RAU, ignored */
			","	/* Requested_GPRS-READY-timer, ignored */
		",\"%8[0-1]\""	/* Requested_Periodic-TAU */
		",\"%8[0-1]\"",	/* Requested_Active-Time */
		&psm_enabled,
		&request_periodic_tau,
		&request_active_time
	);

	if (ret < 0) {
		LOG_ERR("Could not parse PSM data, err %d", ret);
		return;
	}

	if (ret > 0) { /* One param matched */
		LOG_INF("  PSM: %s", psm_enabled ? "enabled" : "disabled");
	}
	if (ret > 1) { /* Two params matched */
		LOG_INF("  Periodic TAU string: %.*s",
		       sizeof(request_periodic_tau), request_periodic_tau);
	}
	if (ret > 2) {  /* Three params matched */
		LOG_INF("  Active time string: %.*s",
			sizeof(request_active_time), request_active_time);
	}
}

static void edrx_read(void)
{
	int ret;
	int act_type;
	char Requested_eDRX_value[4];
	char Provided_eDRX_value[4];
	char Paging_time_window[4];

	LOG_INF("Reading eDRX info...");
	ret = nrf_modem_at_scanf("AT+CEDRXRDP",
		"+CEDRXRDP: "
			"%d"	/* AcT-Type */
		",\"%4[0-1]\""	/* Requested_eDRX_value */
		",\"%4[0-1]\""	/* Provided_eDRX_value */
		",\"%4[0-1]\"",	/* Paging_time_window-Time */
		&act_type,
		&Requested_eDRX_value,
		&Provided_eDRX_value,
		&Paging_time_window
	);
	if (ret < 0) {
		LOG_ERR("Could not parse PSM data, err %d", ret);
		return;
	}

	if (ret > 0) { /* One param matched */
		if (act_type == 0) {
			LOG_INF("  eDRX: disabled");
		} else if (act_type == 4) {
			LOG_INF("  eDRX: enabled, WB-S1 mode");
		} else if (act_type == 5) {
			LOG_INF("  eDRX: enabled, NB-S1 mode");
		}
	}
	if (ret > 1) { /* Two params matched */
		LOG_INF("  Requested eDRX value: %.*s",
		       sizeof(Requested_eDRX_value), Requested_eDRX_value);
	}
	if (ret > 2) {  /* Three params matched */
		LOG_INF("  Provided eDRX value: %.*s",
			sizeof(Provided_eDRX_value), Provided_eDRX_value);
	}
	if (ret > 3) {  /* Four params matched */
		LOG_INF("  Paging time window: %.*s",
			sizeof(Paging_time_window), Paging_time_window);
	}
}

/**
 * @brief Resolve remote host by hostname or IP address
 */
#define PORT_MAX_SIZE    5 /* 0xFFFF = 65535 */
#define PDN_ID_MAX_SIZE  2 /* 0..10 */

int resolve_host(int cid, const char *host, uint16_t port, int family, struct sockaddr *sa)
{
	int err;
	char service[PORT_MAX_SIZE + PDN_ID_MAX_SIZE + 2];
	struct addrinfo *ai = NULL;
	struct addrinfo hints = {
		.ai_flags  = AI_NUMERICSERV | AI_PDNSERV,
		.ai_family = family
	};

	if (sa == NULL) {
		return DNS_EAI_AGAIN;
	}

	/* "service" shall be formatted as follows: "port:pdn_id" */
	snprintf(service, sizeof(service), "%hu:%d", port, cid);
	err = getaddrinfo(host, service, &hints, &ai);
	if (err) {
		return err;
	}

	*sa = *(ai->ai_addr);
	freeaddrinfo(ai);

	if (sa->sa_family != AF_INET && sa->sa_family != AF_INET6) {
		return DNS_EAI_ADDRFAMILY;
	}

	return 0;
}

static struct sockaddr sa = {
	.sa_family = AF_INET
};

static int dns_lookup(void)
{
	int ret;

	ret  = resolve_host(0, "dev.testncs.com", 4567, AF_INET, &sa);
	if (ret) {
		LOG_ERR("getaddrinfo() error: %s", gai_strerror(ret));
		return ret;
	}

	return 0;
}

static void udp_wk(struct k_work *work)
{
	int ret, sock;
#if SBM_NB1_DEMO
	const char tx_data[] = "UDP sending test with RAI enabled";
#else
	const char tx_data[] = "UDP sending test with RAI disabled";
#endif

	ARG_UNUSED(work);

	LOG_INF("[UDP] Test start");
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		LOG_ERR("socket() failed: %d", -errno);
		return;
	}
	ret = sendto(sock, tx_data, strlen(tx_data), 0, &sa, sizeof(struct sockaddr_in));
	if (ret < 0) {
		LOG_ERR("sendto() failed: %d", -errno);
		goto exit_udp;
	}
	LOG_INF("[UDP] >> %s", tx_data);

exit_udp:
	close(sock);
	k_sem_give(&demo_sem);
	LOG_INF("[UDP] Test done");
}

static void tcp_wk(struct k_work *work)
{
	int ret, sock;
	char rx_data[32] = {0};
	const char tx_data[] = "TCP echo test";

	ARG_UNUSED(work);

	LOG_INF("[TCP] Test start");
	tm_start = k_uptime_get();
	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
		LOG_ERR("socket() failed: %d", -errno);
		return;
	}
	ret = connect(sock, &sa, sizeof(struct sockaddr_in));
	if (ret) {
		LOG_ERR("connect() failed: %d", -errno);
		goto exit_tcp;
	}
	metric_tcp_connect_time = k_uptime_delta(&tm_start);
	LOG_INF("[TCP] Connected");
	tm_start = k_uptime_get();
	ret = send(sock, tx_data, strlen(tx_data), 0);
	if (ret < 0) {
		LOG_ERR("send() failed: %d", -errno);
		goto exit_tcp;
	}
	LOG_INF("[TCP] >> %s", tx_data);
	ret = recv(sock, (void *)rx_data, sizeof(rx_data), 0);
	if (ret < 0) {
		LOG_WRN("recv() error: %d", -errno);
	}
	metric_tcp_txrx_time = k_uptime_delta(&tm_start);
	LOG_INF("[TCP] << %s", rx_data);

exit_tcp:
	close(sock);
	k_sem_give(&demo_sem);
	LOG_INF("[TCP] Test done");
}

int sbm_config(void)
{
	int ret;

	ret = nrf_modem_at_printf("AT%%XFACTORYRESET=0");
	if (ret) {
		return ret;
	}

#if SBM_NB1_DEMO
	ret = nrf_modem_at_printf("AT%%XSYSTEMMODE=0,1,0,0");
	if (ret) {
		return ret;
	}
#endif

	ret = nrf_modem_at_printf("AT%%XBANDLOCK=2,\"10000001\"");
	if (ret) {
		return ret;
	}
	ret = nrf_modem_at_printf("AT+CGDCONT=0,\"IP\",\"tiot05\"");
	if (ret) {
		return ret;
	}
	ret = nrf_modem_at_printf("AT+CGAUTH=0,1,\"plusw6q9tattkmpk\",\"msfbbam83bsdetxb\"");
	if (ret) {
		return ret;
	}

	ret = nrf_modem_at_printf("AT%%REDMOB=2");
	if (ret) {
		return ret;
	}

#if SBM_NB1_DEMO
	ret = nrf_modem_at_printf("AT%%XEMPR=0,0,2");
#else
	ret = nrf_modem_at_printf("AT%%XEMPR=1,0,2");
#endif
	if (ret) {
		return ret;
	}

	/* eDRX cycle 20.48s, PTW 5.12s */
#if SBM_NB1_DEMO
	ret = nrf_modem_at_printf("AT+CEDRXS=1,5,\"0010\"");
#else
	ret = nrf_modem_at_printf("AT+CEDRXS=1,4,\"0010\"");
#endif
	if (ret) {
		return ret;
	}

#if SBM_NB1_DEMO
	ret = nrf_modem_at_printf("AT%%XPTW=5,\"0001\"");
#else
	ret = nrf_modem_at_printf("AT%%XPTW=4,\"0011\"");
#endif
	if (ret) {
		return ret;
	}

	return 0;
}

int notify_config(void)
{
	int err;

	err = nrf_modem_at_printf("AT+CEREG=1");
	if (err) {
		return err;
	}
	err = nrf_modem_at_printf("AT%%CESQ=1");
	if (err) {
		return err;
	}
	err = nrf_modem_at_printf("AT%%MDMEV=1");
	if (err) {
		return err;
	}
	err = nrf_modem_at_printf("AT+CSCON=1");
	if (err) {
		return err;
	}
	err = nrf_modem_at_printf("AT%%XMODEMSLEEP=1,0,10240");
	if (err) {
		return err;
	}

	return 0;
}

void main(void)
{
	int err;

	k_work_init_delayable(&udp_work, udp_wk);
	k_work_init_delayable(&tcp_work, tcp_wk);

#if SBM_NB1_DEMO
	LOG_INF("Config SoftBank NB-IoT network");
#else
	LOG_INF("Config SoftBank LTE-M network");
#endif
	err = sbm_config();
	if (err) {
		LOG_ERR("SBM config failed");
		return;
	}
	err = notify_config();
	if (err) {
		LOG_ERR("Notify config failed");
		return;
	}

	LOG_INF("Connecting to network");
	tm_start = k_uptime_get();
	err = nrf_modem_at_printf("AT+CFUN=1");
	if (err) {
		LOG_ERR("AT+CFUN failed");
		return;
	}

	LOG_INF("Waiting for network, timeout 60 sec");
	err = k_sem_take(&demo_sem, K_SECONDS(60));
	k_sem_reset(&demo_sem);

	if (cereg_status == HOME) {
		LOG_INF("Network connection ready");
	} else {
		if (err == -EAGAIN) {
			LOG_WRN("Network connection timed out");
		}
		return;
	}
	/* Wait for RRC release to measure Inactvity time */
	tm_start = k_uptime_get();
	k_sem_take(&rrc_sem, K_FOREVER);
	metric_inactivity_time = k_uptime_delta(&tm_start);

	/* DNS look-up, UDP turnaround time */
	tm_start = k_uptime_get();
	err = dns_lookup();
	metric_udp_dns_time = k_uptime_delta(&tm_start);
	if (err) {
		LOG_ERR("Failed in DNS lookup");
		goto err_exit;
	}
	LOG_INF("DNS look-up done");

	edrx_read();
	k_sem_take(&sleep_sem, K_FOREVER);
	k_sem_reset(&sleep_sem);
	k_sem_take(&sleep_sem, K_FOREVER);
	k_sem_reset(&sleep_sem);
	k_sem_take(&sleep_sem, K_FOREVER);
	k_sem_reset(&sleep_sem);
#if SBM_NB1_DEMO
	(void)nrf_modem_at_printf("AT%%XRAI=4");
	rai_enabled = true;
#endif
	/* UDP send during eDRX sleep with 10s delay */
	k_work_schedule(&udp_work, K_MSEC(10000));
	k_sem_take(&demo_sem, K_FOREVER);
	k_sem_reset(&demo_sem);
	k_sem_take(&sleep_sem, K_FOREVER);
	k_sem_reset(&sleep_sem);
#if SBM_NB1_DEMO
	(void)nrf_modem_at_printf("AT%%XRAI=0");
#endif
	err = nrf_modem_at_printf("AT+CEDRXS=0");
	if (err) {
		LOG_ERR("Failed to disable eDRX");
		goto err_exit;

	}

	/* PSM, T3412 120s, Active time 30s */
	err = nrf_modem_at_printf("AT+CPSMS=1,\"\",\"\",\"10100010\",\"00001111\"");;
	if (err) {
		LOG_ERR("Failed to enable PSM");
		goto err_exit;

	}
	k_sem_take(&rrc_sem, K_FOREVER);
	psm_read();
	(void)nrf_modem_at_printf("AT%%XT3412=1,0,10240");

	k_sem_take(&sleep_sem, K_FOREVER);
	k_sem_reset(&sleep_sem);
	/* TCP send/receive during PSM sleep with 30s delay */
	k_work_schedule(&tcp_work, K_MSEC(30000));
	k_sem_take(&demo_sem, K_FOREVER);

err_exit:
	LOG_INF("Shutting down modem");
	err = nrf_modem_at_printf("AT+CFUN=0");
	if (err) {
		LOG_ERR("AT+CFUN failed");
	}
	nrf_modem_lib_shutdown();

	LOG_INF("=============================================");
	LOG_INF("Attach network time:\t\t\%d.%03d sec", 
		(int)(metric_attach_time/1000), (int)(metric_attach_time%1000));
	LOG_INF("RRC inactivity time:\t\t\%d.%03d sec", 
		(int)(metric_inactivity_time/1000), (int)(metric_inactivity_time%1000));
	LOG_INF("UDP DNS look-up time:\t\t\%d.%03d sec", 
		(int)(metric_udp_dns_time/1000), (int)(metric_udp_dns_time%1000));
	LOG_INF("UDP RAI release time:\t\t\%d.%03d sec", 
		(int)(metric_udp_rai_time/1000), (int)(metric_udp_rai_time%1000));
	LOG_INF("TCP connection time:\t\t\%d.%03d sec", 
		(int)(metric_tcp_connect_time/1000), (int)(metric_tcp_connect_time%1000));
	LOG_INF("TCP echo around time:\t\t\%d.%03d sec", 
		(int)(metric_tcp_txrx_time/1000), (int)(metric_tcp_txrx_time%1000));
	LOG_INF("RRC connected time:\t\t\%d.%03d sec", 
		(int)(rrc_connect_time/1000), (int)(rrc_connect_time%1000));
	LOG_INF("\nDone");
}
