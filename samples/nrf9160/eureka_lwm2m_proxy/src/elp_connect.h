/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef ELP_CONNECT_H_
#define ELP_CONNECT_H_

/**@file elp_connect.h
 *
 * @brief nRF52/nRF91 inter-connect protocol.
 * @{
 */

#include <stdlib.h>
#include <zephyr/types.h>

/** Command type. */
enum ic_cmd_type {
	/** Arbitrary data. */
	CMD_TYPE_DATA = 0x00,
	/** Generic commands service. */
	CMD_TYPE_GENERIC_BASE = 0x01,
	CMD_TYPE_SYNC_CMD = CMD_TYPE_GENERIC_BASE,
	CMD_TYPE_SLEEP_CMD,
	/** Modem control service */
	CMD_TYPE_MDM_BASE = 0x10,
	CMD_TYPE_MDM_INT_CONNECT = CMD_TYPE_MDM_BASE,
	CMD_TYPE_MDM_GO_OFFLINE,
	CMD_TYPE_MDM_POWER_OFF,
	CMD_TYPE_MDM_GO_ONLINE,
	CMD_TYPE_MDM_PSM_REQ,
	CMD_TYPE_MDM_EDRX_REQ,
	/** LWM2M service **/
	CMD_TYPE_LWM2M_BASE = 0x20,
	CMD_TYPE_LWM2M_CONNECT = CMD_TYPE_LWM2M_BASE,
	CMD_TYPE_LWM2M_DISCONNECT,
	CMD_TYPE_LWM2M_SET_PATH,
	CMD_TYPE_LWM2M_READ_INT,
	CMD_TYPE_LWM2M_WRITE_INT,
	CMD_TYPE_LWM2M_READ_FLOAT,
	CMD_TYPE_LWM2M_WRITE_FLOAT,
	CMD_TYPE_LWM2M_READ_STRING,
	CMD_TYPE_LWM2M_WRITE_STRING,
	CMD_TYPE_LWM2M_READ_OPAQUE,
	CMD_TYPE_LWM2M_WRITE_OPAQUE,
	/** Type reserved. */
	CMD_TYPE_RESERVED = 0x7F,
	/** Type response base. */
	RSP_TYPE_BASE = 0x80,
	/** Type unsocilicted notification */
	RSP_TYPE_NOTIFICATION = 0xFF
};

/** Unsolicited notification type. */
enum ic_notify_type {
	NOT_TYPE_BASE = 0x00,
	/** Notification type LwM2M */
	NOT_TYPE_LWM2M_RD,	/* Registration and discovery events */
	NOT_TYPE_LWM2M_OBJECT,	/* LwM2M Object events */
	NOT_TYPE_INVALID
};

/**
 * @typedef data_handler_t
 * @brief Callback when data is received from inter_connect interface.
 *
 * @param data_type Type of data, see ic_cmd_type.
 * @param data_buf Data buffer.
 * @param data_len Length of data buffer.
 */
typedef void (*data_handler_t)(u8_t data_type, const u8_t *data_buf,
				u8_t data_len);

/** @brief Initialize the library.
 *
 * @param data_handler Callback handler for received data.
 *
 * @retval 0 If the operation was successful.
 *	Otherwise, a (negative) error code is returned.
 */
int inter_connect_init(data_handler_t data_handler);

/** @brief Uninitialize the library.
 *
 * @retval 0 If the operation was successful.
 *	Otherwise, a (negative) error code is returned.
 */
int inter_connect_uninit(void);

/** @brief Send data or command response to the peer.
 *
 * @param data_type Type of response, see ic_cmd_type.
 * @param data_buf Data buffer.
 * @param data_len Length of data buffer.
 *
 * @retval 0 If the operation was successful.
 *	Otherwise, a (negative) error code is returned.
 */
int inter_connect_send(u8_t type, const u8_t *data_buf, u8_t data_len);

/** @brief Send unsolicited response to the peer.
 *
 * @param data_type Type of notification, see ic_notify_type.
 * @param data_buf Data buffer.
 * @param data_len Length of data buffer.
 *
 * @retval 0 If the operation was successful.
 *	Otherwise, a (negative) error code is returned.
 */
int inter_connect_notify(u8_t type, const u8_t *data_buf, u8_t data_len);

/** @} */

#endif /* ELP_CONNECT_H_ */
