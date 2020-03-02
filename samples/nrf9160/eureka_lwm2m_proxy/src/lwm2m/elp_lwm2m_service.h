/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef TH_LWM2M_SERVICE_
#define TH_LWM2M_SERVICE_

/**@file th_lwm2m_service.h
 *
 * @brief LwM2M control service in serial LTE modem
 * @{
 */

#include <stdlib.h>
#include <zephyr/types.h>

/**
 * @brief LwM2M control command received from inter_connect interface.
 *
 * @param cmd     Type of LwM2M control command, see ic_cmd_type.
 * @param param   Parameter data for command.
 * @param length  Length of parameter data.
 */
void th_lwm2m_control(u8_t cmd, const u8_t *param, uint8_t length);

/** @} */

#endif /* TH_LWM2M_SERVICE_ */
