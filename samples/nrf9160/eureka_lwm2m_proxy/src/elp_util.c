/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#include <logging/log.h>
#include <zephyr.h>
#include <stdio.h>
#include <string.h>

#include <net/socket.h>

LOG_MODULE_REGISTER(util, CONFIG_ELP_LOG_LEVEL);

/**@brief Resolves host IPv4 address and port
 */
bool util_check_for_ipv4(const char *address, u8_t length)
{
	int index;

	for (index = 0; index < length; index++) {
		char ch = *(address + index);
		if ( (ch == '.') || (ch >= '0' && ch <= '9')) {
			continue;
		} else {
			return false;
		}
	}

	return true;
}

/**@brief Resolves host IPv4 address and port
 */
int util_parse_host_by_ipv4(const char *ip, u16_t port, void *socket_addr)
{
	struct sockaddr_in *server4 = ((struct sockaddr_in *)socket_addr);

	server4->sin_family = AF_INET;
	server4->sin_port = htons(port);
	LOG_INF("IPv4 Address %s", log_strdup(ip));
	/* NOTE inet_pton() returns 1 as success */
	if (inet_pton(AF_INET, ip, &server4->sin_addr) == 1) {
		return 0;
	} else {
		return -EINVAL;
	}
}

/**@brief Resolves hostname and port
 */
int util_parse_host_by_name(const char *name, u16_t port, 
			enum net_sock_type socktype, void *socket_addr)
{
	int err;
	struct addrinfo *result;
	struct addrinfo hints = {
		.ai_family = AF_INET,
		.ai_socktype = socktype
	};
	char ipv4_addr[NET_IPV4_ADDR_LEN];

	err = getaddrinfo(name, NULL, &hints, &result);
	if (err) {
		LOG_ERR("ERROR: getaddrinfo failed %d", err);
		return err;
	}

	if (result == NULL) {
		LOG_ERR("ERROR: Address not found\n");
		return -ENOENT;
	}

	/* IPv4 Address. */
	struct sockaddr_in *server4 = ((struct sockaddr_in *)socket_addr);

	server4->sin_addr.s_addr =
		((struct sockaddr_in *)result->ai_addr)->sin_addr.s_addr;
	server4->sin_family = AF_INET;
	server4->sin_port = htons(port);

	inet_ntop(AF_INET, &server4->sin_addr.s_addr, ipv4_addr,
		  sizeof(ipv4_addr));
	LOG_DBG("IPv4 Address found %s\n", ipv4_addr);

	/* Free the address. */
	freeaddrinfo(result);

	return 0;
}

