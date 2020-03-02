/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef TH_MODEM_SERVICE_
#define TH_MODEM_SERVICE_

/**@file th_modem_service.h
 *
 * @brief Modem control service in serial LTE modem
 * @{
 */

#include <stdlib.h>
#include <zephyr/types.h>

/**
 * @brief Modem control command received from inter_connect interface.
 *
 * @param cmd     Type of modem control command, see ic_cmd_type.
 * @param param   Parameter data for command.
 */
void th_modem_control(u8_t cmd, const u8_t *param);

/** @} */

#endif /* TH_MODEM_SERVICE_ */
