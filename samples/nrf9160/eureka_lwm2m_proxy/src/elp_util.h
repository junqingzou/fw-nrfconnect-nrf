/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef TH_UTIL_
#define TH_UTIL_

/**@file th_util.h
 *
 * @brief Common utility functions for serial LTE modem
 * @{
 */

/** @brief Check whether an remote address is IPv4 address of not.
 *
 * @param address Hostname or IPv4 address string.
 * @param length Length of the address string
 *
 * @retval true If the address is an IPv4 address.
 *	Otherwise, false is it's a hostname.
 */
bool util_check_for_ipv4(const char *address, u8_t length);

/** @brief Resolve IP address to socket address
 *
 * @param ip Remote IP Address string.
 * @param port Remote port number.
 * @param socket_addr Output of the resolved socket address.
 *
 * @retval 0 If the operation was successful.
 *	Otherwise, a (negative) error code is returned.
 */
int util_parse_host_by_ipv4(const char *ip, u16_t port, void *socket_addr);

/** @brief Resolve hostname to socket address
 *
 * @param ip Remote hostname string.
 * @param port Remote port number.
 * @param socket_addr Output of the resolved socket address.
 *
 * @retval 0 If the operation was successful.
 *	Otherwise, a (negative) error code is returned.
 */
int util_parse_host_by_name(const char *name, u16_t port,
			enum net_sock_type socktype, void *socket_addr);

/** @} */

#endif /* TH_UTIL_ */
