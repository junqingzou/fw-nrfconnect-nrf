/*
 * Copyright (c) 2018 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <stdio.h>
//include <drivers/uart.h>
#include <string.h>
#include <net/mqtt.h>
#include <net/socket.h>
#include <net/ftp_client.h>
#include <modem/lte_lc.h>

#define HOST_NAME	"speedtest.tele2.net"
#define FTP_PORT	21
#define USER_NAME	"anonymous"
#define USER_PASSWORD	"anonymous@example.com"
#define SEC_TAG		-1

#define TARGET_FILE	"1MB.zip"
#define TARGET_SIZE	1024*1024

static uint64_t start_time, now_time;
static bool first_data_received;
static int bytes_received;
static K_SEM_DEFINE(rx_done, 0, 1);

#if defined(CONFIG_BSD_LIBRARY)

/**@brief Recoverable BSD library error. */
void bsd_recoverable_error_handler(uint32_t err)
{
	printk("bsdlib recoverable error: %u\n", (unsigned int)err);
}

#endif /* defined(CONFIG_BSD_LIBRARY) */

/**@brief Configures modem to provide LTE link. Blocks until link is
 * successfully established.
 */
static void modem_configure(void)
{
#if defined(CONFIG_LTE_LINK_CONTROL)
	if (IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT)) {
		/* Do nothing, modem is already turned on
		 * and connected.
		 */
	} else {
		int err;

		printk("LTE Link Connecting ...\n");
		err = lte_lc_init_and_connect();
		__ASSERT(err == 0, "LTE link could not be established.");
		printk("LTE Link Connected!\n");
	}
#endif /* defined(CONFIG_LTE_LINK_CONTROL) */
}

void ftp_ctrl_callback(const uint8_t *msg, uint16_t len)
{
	printk("%s", (char *)msg);
}

void ftp_data_callback(const uint8_t *msg, uint16_t len)
{
	now_time = k_uptime_get();
	if (!first_data_received) {
		start_time = now_time;
		first_data_received = true;
	}
	printk("[%08d] %d bytes received\n", (int)(now_time - start_time), len);
	bytes_received += len;
	if (bytes_received >= TARGET_SIZE) {
		now_time = k_uptime_delta(&start_time);
		printk("done, run-time: %d ms\n", (int)now_time);
		k_sem_give(&rx_done);
	}
}

void main(void)
{
	int ret;

	printk("FTP speed test sample started\n");

	modem_configure();
	ftp_init(ftp_ctrl_callback, ftp_data_callback);

	/* FTP open */
	ret = ftp_open(HOST_NAME, FTP_PORT, SEC_TAG);
	if (ret != FTP_CODE_200) {
		printk("ftp_open error %d", ret);
		return;
	}
	/* FTP login */
	ret = ftp_login(USER_NAME, USER_PASSWORD);
	if (ret != FTP_CODE_230) {
		printk("ftp_login error %d", ret);
		ftp_close();
		return;
	}
#if 0
	/* FTP list*/
	ret = ftp_list("-l", TARGET_FILE);
	if (ret != FTP_CODE_226) {
		printk("ftp_list error %d", ret);
		ftp_close();
		return;
	}
#endif
	/* FTP get */
	first_data_received = false;
	ftp_get(TARGET_FILE);
	k_sem_take(&rx_done, K_FOREVER);
	ftp_close();
	k_sleep(K_SECONDS(1));
	lte_lc_power_off();
	int total_time = (now_time + 500) / 1000;
	int total_bits = bytes_received * 8;
	printk("============================\n");
	printk("time:\t\t%d sec\n", total_time);
	printk("size:\t\t%d bits\n", total_bits);
	printk("throughput:\t%d bps\n", total_bits / total_time);
}
