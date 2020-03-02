/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#include <logging/log.h>
#include <zephyr.h>
#include <stdio.h>
#include <string.h>
#include <drivers/uart.h>

#include "elp_connect.h"

#define CONFIG_UART_0_NAME	 "UART_0"
#define CONFIG_UART_1_NAME	 "UART_1"
#define CONFIG_UART_2_NAME	 "UART_2"

LOG_MODULE_REGISTER(conn, CONFIG_ELP_LOG_LEVEL);

/**
 * Data packet format
 * [STX(1)][TYPE(1)][LENGTH(1)][VALUE(vary)][BCC(1)]
 */
#define PROT_HEADER_STX		0x02	/** STX char */
#define PROT_HEADER_LEN		3	/** [STX][TYPE][LENGTH]*/
#define PROT_STX_POS		0	/** Position of STX byte */
#define PROT_TYP_POS		1	/** Position of Type byte */
#define PROT_LEN_POS		2	/** Position of Length byte */

/** @brief UARTs. */
enum select_uart {
	UART_0,
	UART_1,
	UART_2
};

static data_handler_t data_handler_cb;
static struct device *uart_dev;
static u8_t rx_buff[CONFIG_INTER_CONNECT_UART_BUF_SIZE];
static u8_t tx_buff[CONFIG_INTER_CONNECT_UART_BUF_SIZE];
static bool module_initialized;

static struct k_work rx_data_handle_work;

static void rx_data_handle(struct k_work *work)
{
	ARG_UNUSED(work);

	if (data_handler_cb == NULL) {
		LOG_ERR("Not itialized");
		return;
	}

	LOG_HEXDUMP_DBG(rx_buff, (rx_buff[PROT_LEN_POS]+PROT_HEADER_LEN+1),
		"RX");

	if (rx_buff[PROT_STX_POS] != PROT_HEADER_STX) {
		LOG_ERR("Missing STX");
		return;
	}
#if CONFIG_INTER_CONNECT_BCC
	{
		u8_t bcc = 0xff;
		u8_t *ptr = &rx_buff[PROT_LEN_POS+1];
		size_t i;

		for (i = PROT_TYP_POS; i < (rx_buff[PROT_LEN_POS]-1); i++) {
			bcc ^= *ptr;
			ptr++;
		}
		if (bcc != *ptr) {
			LOG_ERR("BCC error");
			return;
		}
	}
#endif	/* CONFIG_INTER_CONNECT_BCC */

	data_handler_cb(rx_buff[PROT_TYP_POS],
			&rx_buff[PROT_LEN_POS+1],
			rx_buff[PROT_LEN_POS]);
	uart_irq_rx_enable(uart_dev);
}

static void uart_rx_handler(u8_t character)
{
	static size_t pkt_length;
	static size_t cmd_len;
	size_t pos;

	pos = cmd_len;
	cmd_len += 1;
	if (pos == PROT_LEN_POS) {
		pkt_length = character;
	}

	/* Detect buffer overflow or zero length */
	if (cmd_len > CONFIG_INTER_CONNECT_UART_BUF_SIZE) {
		LOG_ERR("Buffer overflow, dropping '%c'", character);
		cmd_len = CONFIG_INTER_CONNECT_UART_BUF_SIZE;
		return;
	} else if (cmd_len < 1) {
		LOG_ERR("Invalid packet length: %d", cmd_len);
		cmd_len = 0;
		return;
	}

	rx_buff[pos] = character;

	/* Check if all packet is received. */
	if (pos == (PROT_LEN_POS + pkt_length + 1)) {
		goto send;
	}
	return;

send:
	uart_irq_rx_disable(uart_dev);
	k_work_submit(&rx_data_handle_work);
	cmd_len = 0;
	pkt_length = 0;
}

static void if_isr(struct device *dev)
{
	u8_t character;

	uart_irq_update(dev);

	if (uart_irq_rx_ready(dev)) {
		/* keep reading until drain all */
		while (uart_fifo_read(dev, &character, 1)) {
			uart_rx_handler(character);
		}
	}
}

static int if_uart_init(char *uart_dev_name)
{
	int err;

	uart_dev = device_get_binding(uart_dev_name);
	if (uart_dev == NULL) {
		LOG_ERR("Cannot bind %s", uart_dev_name);
		return -EINVAL;
	}
	err = uart_err_check(uart_dev);
	if (err) {
		LOG_WRN("UART check failed: %d", err);
	}
	uart_irq_callback_set(uart_dev, if_isr);
	return 0;
}

int inter_connect_init(data_handler_t data_handler)
{
	char *uart_dev_name;
	int err;
	enum select_uart uart_id = CONFIG_INTER_CONNECT_UART;

	if (data_handler == NULL) {
		LOG_ERR("Data handler is null");
		return -EINVAL;
	}
	data_handler_cb = data_handler;

	/* Choose which UART to use */
	switch (uart_id) {
	case UART_0:
		uart_dev_name = CONFIG_UART_0_NAME;
		break;
	case UART_1:
		uart_dev_name = CONFIG_UART_1_NAME;
		break;
	case UART_2:
		uart_dev_name = CONFIG_UART_2_NAME;
		break;
	default:
		LOG_ERR("Unknown UART instance %d", uart_id);
		return -EINVAL;
	}

	/* Initialize the UART module */
	err = if_uart_init(uart_dev_name);
	if (err) {
		LOG_ERR("UART could not be initialized: %d", err);
		return -EFAULT;
	}

	k_work_init(&rx_data_handle_work, rx_data_handle);
	memset(rx_buff, 0x00, CONFIG_INTER_CONNECT_UART_BUF_SIZE);
	uart_irq_rx_enable(uart_dev);
	module_initialized = true;
	return err;
}

int inter_connect_uninit(void)
{
	int err = 0;

#if defined(CONFIG_DEVICE_POWER_MANAGEMENT)
	err = device_set_power_state(uart_dev, DEVICE_PM_OFF_STATE,
				NULL, NULL);
	if (err != 0) {
		LOG_WRN("Can't power off uart err=%d", err);
	}
#endif
	return 0;
}

int inter_connect_send(u8_t type, const u8_t *data_buff, u8_t data_len)
{
	u32_t pkt_size = data_len + PROT_HEADER_LEN + 1;

	if (!module_initialized) {
		return -EACCES;
	}

	if (pkt_size > CONFIG_INTER_CONNECT_UART_BUF_SIZE) {
		LOG_ERR("Message size error");
		return -EINVAL;
	}

	/* Assemble the protocol message */
	memset(tx_buff, 0x00, CONFIG_INTER_CONNECT_UART_BUF_SIZE);
	tx_buff[PROT_STX_POS] = PROT_HEADER_STX;
	tx_buff[PROT_TYP_POS] = type;
	tx_buff[PROT_LEN_POS] = data_len;
	if (data_buff != NULL) {
		memcpy(&tx_buff[PROT_LEN_POS+1], data_buff, data_len);
	}

	LOG_HEXDUMP_DBG(tx_buff, pkt_size, "TX");
	/* Forward the data over UART if any. */
	/* Poll out what is in the buffer gathered from the modem. */
	for (size_t i = 0; i < pkt_size; i++) {
		uart_poll_out(uart_dev, tx_buff[i]);
	}

	return 0;
}

int inter_connect_notify(u8_t type, const u8_t *data_buff, u8_t data_len)
{
	u32_t pkt_size = data_len + PROT_HEADER_LEN + 2;

	if (!module_initialized) {
		return -EACCES;
	}

	if (pkt_size > CONFIG_INTER_CONNECT_UART_BUF_SIZE) {
		LOG_ERR("Message size error");
		return -EINVAL;
	}

	/* Assemble the protocol message */
	memset(tx_buff, 0x00, CONFIG_INTER_CONNECT_UART_BUF_SIZE);
	tx_buff[PROT_STX_POS] = PROT_HEADER_STX;
	tx_buff[PROT_TYP_POS] = RSP_TYPE_NOTIFICATION;
	tx_buff[PROT_LEN_POS] = data_len+1;
	tx_buff[PROT_LEN_POS+1] = type;
	if (data_buff != NULL) {
		memcpy(&tx_buff[PROT_LEN_POS+2], data_buff, data_len);
	}

	LOG_HEXDUMP_DBG(tx_buff, pkt_size, "TX");
	/* Forward the data over UART if any. */
	/* Poll out what is in the buffer gathered from
	 * the modem.
	 */
	for (size_t i = 0; i < pkt_size; i++) {
		uart_poll_out(uart_dev, tx_buff[i]);
	}

	return 0;
}
