/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <logging/log.h>
#include <zephyr.h>
#include <stdio.h>
#include <string.h>
#include <net/socket.h>
#include <modem/modem_key_mgmt.h>
#include <net/tls_credentials.h>
#include "slm_util.h"
#include "slm_at_host.h"
#include "slm_at_tcpip.h"
#include "slm_native_tls.h"

LOG_MODULE_REGISTER(sock, CONFIG_SLM_LOG_LEVEL);

/* Some features need lib nrf_modem update */
#define SOCKET_FUTURE_FEATURE 0

/*
 * Known limitation in this version
 * - Multiple concurrent sockets
 * - TCP server accept one connection only
 * - Receive more than IPv4 MTU one-time
 */

/**@brief Socket operations. */
enum slm_socket_operation {
	AT_SOCKET_CLOSE,
	AT_SOCKET_OPEN_IPV4,
	AT_SOCKET_OPEN_IPV6
};

/**@brief Socketopt operations. */
enum slm_socketopt_operation {
	AT_SOCKETOPT_GET,
	AT_SOCKETOPT_SET
};

/**@brief Socket roles. */
enum slm_socket_role {
	AT_SOCKET_ROLE_CLIENT,
	AT_SOCKET_ROLE_SERVER
};

static struct {
	uint16_t type;     /* SOCK_STREAM or SOCK_DGRAM */
	uint16_t role;     /* Client or Server */
	sec_tag_t sec_tag; /* Security tag of the credential */
	uint16_t hostname_verify; /* (D)TLS verify hostname or not */

	int family;        /* Socket address family */
	int fd;            /* Socket descriptor. */
	int fd_peer;       /* Socket descriptor for peer. */
} sock;

/* global functions defined in different files */
void rsp_send(const uint8_t *str, size_t len);

/* global variable defined in different files */
extern struct at_param_list at_param_list;
extern char rsp_buf[CONFIG_SLM_SOCKET_RX_MAX * 2];
extern uint8_t rx_data[CONFIG_SLM_SOCKET_RX_MAX];

static int do_socket_open(void)
{
	int proto = IPPROTO_TCP;

	if (sock.fd != INVALID_SOCKET) {
		LOG_WRN("Socket is already opened");
		return -EINVAL;
	}

	if (sock.type == SOCK_STREAM) {
		sock.fd = socket(sock.family, SOCK_STREAM, IPPROTO_TCP);
	} else if (sock.type == SOCK_DGRAM) {
		sock.fd = socket(sock.family, SOCK_DGRAM, IPPROTO_UDP);
		proto = IPPROTO_UDP;
	} else {
		LOG_ERR("socket type %d not supported", sock.type);
		return -ENOTSUP;
	}
	if (sock.fd < 0) {
		LOG_ERR("socket() error: %d", -errno);
		return -errno;
	}

	sprintf(rsp_buf, "\r\n#XSOCKET: %d,%d,%d,%d\r\n", sock.fd, sock.type, sock.role, proto);
	rsp_send(rsp_buf, strlen(rsp_buf));

	return 0;
}

static int do_secure_socket_open(uint16_t peer_verify)
{
	int ret = 0;
	int proto = IPPROTO_TLS_1_2;

	if (sock.fd != INVALID_SOCKET) {
		LOG_WRN("Secure socket is already opened");
		return -EINVAL;
	}

	if (sock.type == SOCK_STREAM) {
		sock.fd = socket(sock.family, SOCK_STREAM, IPPROTO_TLS_1_2);
	} else if (sock.type == SOCK_DGRAM) {
		sock.fd = socket(sock.family, SOCK_DGRAM, IPPROTO_DTLS_1_2);
		proto = IPPROTO_DTLS_1_2;
	} else {
		LOG_ERR("socket type %d not supported", sock.type);
		return -ENOTSUP;
	}
	if (sock.fd < 0) {
		LOG_ERR("socket() error: %d", -errno);
		return -errno;
	}

	sec_tag_t sec_tag_list[1] = { sock.sec_tag };
#if defined(CONFIG_SLM_NATIVE_TLS)
	ret = slm_tls_loadcrdl(sock.sec_tag);
	if (ret < 0) {
		LOG_ERR("Fail to load credential: %d", ret);
		ret = -EAGAIN;
		goto error_exit;
	}
#endif
	ret = setsockopt(sock.fd, SOL_TLS, TLS_SEC_TAG_LIST, sec_tag_list, sizeof(sec_tag_t));
	if (ret) {
		LOG_ERR("setsockopt(TLS_SEC_TAG_LIST) error:: %d", -errno);
		ret = -errno;
		goto error_exit;
	}

	/* Set up (D)TLS peer verification */
	ret = setsockopt(sock.fd, SOL_TLS, TLS_PEER_VERIFY, &peer_verify, sizeof(peer_verify));
	if (ret) {
		LOG_ERR("setsockopt(TLS_PEER_VERIFY) error: %d", errno);
		ret = -errno;
		goto error_exit;
	}

	sprintf(rsp_buf, "\r\n#XSSOCKET: %d,%d,%d,%d\r\n", sock.fd, sock.type, sock.role, proto);
	rsp_send(rsp_buf, strlen(rsp_buf));

	return 0;

error_exit:
	close(sock.fd);
	slm_at_tcpip_init();

	return ret;
}

static int do_socket_close(int error)
{
	int ret;

	if (sock.fd == INVALID_SOCKET) {
		return 0;
	}

#if defined(CONFIG_SLM_NATIVE_TLS)
	if (sock.sec_tag != INVALID_SEC_TAG) {
		ret = slm_tls_unloadcrdl(sock.sec_tag);
		if (ret < 0) {
			LOG_ERR("Fail to load credential: %d", ret);
			return ret;
		}
	}
#endif
	if (sock.fd_peer != INVALID_SOCKET) {
		ret = close(sock.fd_peer);
		if (ret) {
			LOG_WRN("peer close() error: %d", -errno);
		}
	}
	ret = close(sock.fd);
	if (ret) {
		LOG_WRN("close() error: %d", -errno);
		ret = -errno;
	}
	slm_at_tcpip_init();
	sprintf(rsp_buf, "\r\n#XSOCKET: %d,\"closed\"\r\n", error);
	rsp_send(rsp_buf, strlen(rsp_buf));

	return ret;
}

static int do_socketopt_set_str(int option, const char *value)
{
	int ret = -ENOTSUP;

	switch (option) {
	case SO_BINDTODEVICE:
		ret = setsockopt(sock.fd, SOL_SOCKET, option, value, strlen(value));
		if (ret < 0) {
			LOG_ERR("setsockopt(%d) error: %d", option, -errno);
		}
		break;

	default:
		LOG_WRN("Unknown option %d", option);
		break;
	}

	return ret;
}

static int do_socketopt_set_int(int option, int value)
{
	int ret = -ENOTSUP;

	switch (option) {
	case SO_REUSEADDR:
		ret = setsockopt(sock.fd, SOL_SOCKET, option, &value, sizeof(int));
		if (ret < 0) {
			LOG_ERR("setsockopt(%d) error: %d", option, -errno);
		}
		break;

	case SO_RCVTIMEO:
	case SO_SNDTIMEO: {
		struct timeval tmo = { .tv_sec = value };
		socklen_t len = sizeof(struct timeval);

		ret = setsockopt(sock.fd, SOL_SOCKET, option, &tmo, len);
		if (ret < 0) {
			LOG_ERR("setsockopt(%d) error: %d", option, -errno);
		}
	} break;

	/** NCS extended socket options */
	case SO_SILENCE_ALL:
	case SO_IP_ECHO_REPLY:
	case SO_IPV6_ECHO_REPLY:
	case SO_TCP_SRV_SESSTIMEO:
		ret = setsockopt(sock.fd, SOL_SOCKET, option, &value, sizeof(int));
		if (ret < 0) {
			LOG_ERR("setsockopt(%d) error: %d", option, -errno);
		}
		break;

	/* RAI-related */
	case SO_RAI_LAST:
	case SO_RAI_NO_DATA:
	case SO_RAI_ONE_RESP:
	case SO_RAI_ONGOING:
	case SO_RAI_WAIT_MORE:
		ret = setsockopt(sock.fd, SOL_SOCKET, option, NULL, 0);
		if (ret < 0) {
			LOG_ERR("setsockopt(%d) error: %d", option, -errno);
		}
		break;


	case SO_PRIORITY:
	case SO_TIMESTAMPING:
		sprintf(rsp_buf, "\r\n#XSOCKETOPT: \"not supported\"\r\n");
		rsp_send(rsp_buf, strlen(rsp_buf));
		break;

	default:
		LOG_WRN("Unknown option %d", option);
		break;
	}

	return ret;
}

static int do_socketopt_get(int option)
{
	int ret = 0;

	switch (option) {
	case SO_SILENCE_ALL:
	case SO_IP_ECHO_REPLY:
	case SO_IPV6_ECHO_REPLY:
	case SO_TCP_SRV_SESSTIMEO:
	case SO_ERROR: {
		int value;
		socklen_t len = sizeof(int);

		ret = getsockopt(sock.fd, SOL_SOCKET, option, &value, &len);
		if (ret) {
			LOG_ERR("getsockopt(%d) error: %d", option, -errno);
		} else {
			sprintf(rsp_buf, "\r\n#XSOCKETOPT: %d\r\n", value);
			rsp_send(rsp_buf, strlen(rsp_buf));
		}
	} break;

	case SO_RCVTIMEO:
	case SO_SNDTIMEO: {
		struct timeval tmo;
		socklen_t len = sizeof(struct timeval);

		ret = getsockopt(sock.fd, SOL_SOCKET, option, &tmo, &len);
		if (ret) {
			LOG_ERR("getsockopt(%d) error: %d", option, -errno);
		} else {
			sprintf(rsp_buf, "\r\n#XSOCKETOPT: \"%d sec\"\r\n", (int)tmo.tv_sec);
			rsp_send(rsp_buf, strlen(rsp_buf));
		}
	} break;

	case SO_TYPE:
	case SO_PRIORITY:
	case SO_PROTOCOL:
		sprintf(rsp_buf, "\r\n#XSOCKETOPT: \"not supported\"\r\n");
		rsp_send(rsp_buf, strlen(rsp_buf));
		break;

	default:
		LOG_WRN("Unknown option %d", option);
		break;
	}

	return ret;
}

static int do_bind(uint16_t port)
{
	int ret;

	if (sock.family == AF_INET) {
		char ipv4_addr[INET_ADDRSTRLEN] = {0};

		util_get_ip_addr(ipv4_addr, NULL);
		if (strlen(ipv4_addr) == 0) {
			LOG_ERR("Get local IPv4 address failed");
			return -EINVAL;
		}

		struct sockaddr_in local = {
			.sin_family = AF_INET,
			.sin_port = htons(port)
		};

		if (inet_pton(AF_INET, ipv4_addr, &local.sin_addr) != 1) {
			LOG_ERR("Parse local IPv4 address failed: %d", -errno);
			return -EAGAIN;
		}

		ret = bind(sock.fd, (struct sockaddr *)&local, sizeof(struct sockaddr_in));
		if (ret) {
			LOG_ERR("bind() failed: %d", -errno);
			do_socket_close(-errno);
			return -errno;
		}
		LOG_DBG("bind to %s", log_strdup(ipv4_addr));
	} else if (sock.family == AF_INET6) {
		char ipv6_addr[INET6_ADDRSTRLEN] = {0};

		util_get_ip_addr(NULL, ipv6_addr);
		if (strlen(ipv6_addr) == 0) {
			LOG_ERR("Get local IPv6 address failed");
			return -EINVAL;
		}

		struct sockaddr_in6 local = {
			.sin6_family = AF_INET6,
			.sin6_port = htons(port)
		};

		if (inet_pton(AF_INET6, ipv6_addr, &local.sin6_addr) != 1) {
			LOG_ERR("Parse local IPv6 address failed: %d", -errno);
			return -EAGAIN;
		}
		ret = bind(sock.fd, (struct sockaddr *)&local, sizeof(struct sockaddr_in6));
		if (ret) {
			LOG_ERR("bind() failed: %d", -errno);
			do_socket_close(-errno);
			return -errno;
		}
		LOG_DBG("bind to %s", log_strdup(ipv6_addr));
	} else {
		return -EINVAL;
	}

	return 0;
}

static int do_connect(const char *url, uint16_t port)
{
	int ret = 0;
	struct addrinfo *res;

	LOG_DBG("connect %s:%d", log_strdup(url), port);

	ret = getaddrinfo(url, NULL, NULL, &res);
	if (ret) {
		LOG_ERR("getaddrinfo() error: %s", log_strdup(gai_strerror(ret)));
		return -EAGAIN;
	}

	/* NOTE use first resolved address as target */
	if ((sock.family == AF_INET && res->ai_family != AF_INET) ||
	    (sock.family == AF_INET6 && res->ai_family != AF_INET6)) {
		LOG_ERR("Address family mismatch");
		freeaddrinfo(res);
		return -EINVAL;
	}

	if (sock.sec_tag != INVALID_SEC_TAG) {
		if (sock.hostname_verify) {
			ret = setsockopt(sock.fd, SOL_TLS, TLS_HOSTNAME, url, strlen(url));
			if (ret < 0) {
				LOG_ERR("Failed to set TLS_HOSTNAME (%d)", -errno);
				do_socket_close(-errno);
				return -errno;
			}
#ifdef SOCKET_FUTURE_FEATURE
/* Due to bug report NCSIDB-497, cannot explicitly clear TLS_HOSTNAME yet*/
		} else {
			ret = setsockopt(sock.fd, SOL_TLS, TLS_HOSTNAME, NULL, 0);
			if (ret < 0) {
				LOG_ERR("Failed to clear TLS_HOSTNAME (%d)", -errno);
				do_socket_close(-errno);
				return -errno;
			}
#endif
		}
	}

	if (res->ai_family == AF_INET) {
		struct sockaddr_in *host = (struct sockaddr_in *)res->ai_addr;

		host->sin_port = htons(port);
		ret = connect(sock.fd, (struct sockaddr *)host, sizeof(struct sockaddr_in));
	} else {
		struct sockaddr_in6 *host = (struct sockaddr_in6 *)res->ai_addr;

		host->sin6_port = htons(port);
		ret = connect(sock.fd, (struct sockaddr *)host, sizeof(struct sockaddr_in6));
	}
	if (ret) {
		LOG_ERR("connect() error: %d", -errno);
		do_socket_close(-errno);
		return -errno;
	}

	sprintf(rsp_buf, "\r\n#XCONNECT: 1\r\n");
	rsp_send(rsp_buf, strlen(rsp_buf));
	freeaddrinfo(res);

	return ret;
}

static int do_listen(void)
{
	int ret;

	/* hardcode backlog to be 1 for now */
	ret = listen(sock.fd, 1);
	if (ret < 0) {
		LOG_ERR("listen() error: %d", -errno);
		do_socket_close(-errno);
		return -errno;
	}

	return 0;
}

static int do_accept(void)
{
	char peer_addr[INET6_ADDRSTRLEN] = {0};

	if (sock.family == AF_INET) {
		struct sockaddr_in client;
		socklen_t len = sizeof(struct sockaddr_in);

		/* Blocking call */
		sock.fd_peer = accept(sock.fd, (struct sockaddr *)&client, &len);
		if (sock.fd_peer == -1) {
			LOG_ERR("accept() error: %d", -errno);
			sock.fd_peer = INVALID_SOCKET;
			do_socket_close(-errno);
			return -errno;
		}
		(void)inet_ntop(AF_INET, &client.sin_addr, peer_addr, sizeof(peer_addr));
	} else if (sock.family == AF_INET6) {
		struct sockaddr_in6 client;
		socklen_t len = sizeof(struct sockaddr_in6);

		/* Blocking call */
		sock.fd_peer = accept(sock.fd, (struct sockaddr *)&client, &len);
		if (sock.fd_peer == -1) {
			LOG_ERR("accept() error: %d", -errno);
			sock.fd_peer = INVALID_SOCKET;
			do_socket_close(-errno);
			return -errno;
		}
		(void)inet_ntop(AF_INET6, &client.sin6_addr, peer_addr, sizeof(peer_addr));
	} else {
		return -EINVAL;
	}

	sprintf(rsp_buf, "\r\n#XACCEPT: \"connected with %s\"\r\n", peer_addr);
	rsp_send(rsp_buf, strlen(rsp_buf));

	sprintf(rsp_buf, "\r\n#XACCEPT: %d\r\n", sock.fd_peer);
	rsp_send(rsp_buf, strlen(rsp_buf));

	return 0;
}

static int do_send(const uint8_t *data, int datalen)
{
	int ret = 0;
	int sockfd = sock.fd;

	/* For TCP/TLS Server, send to imcoming socket */
	if (sock.type == SOCK_STREAM && sock.role == AT_SOCKET_ROLE_SERVER) {
		if (sock.fd_peer != INVALID_SOCKET) {
			sockfd = sock.fd_peer;
		} else {
			LOG_ERR("No connection");
			return -EINVAL;
		}
	}

	uint32_t offset = 0;

	while (offset < datalen) {
		ret = send(sockfd, data + offset, datalen - offset, 0);
		if (ret < 0) {
			LOG_ERR("send() failed: %d", -errno);
			if (errno != EAGAIN && errno != ETIMEDOUT) {
				do_socket_close(-errno);
			} else {
				sprintf(rsp_buf, "\r\n#XSOCKET: %d\r\n", -errno);
				rsp_send(rsp_buf, strlen(rsp_buf));
			}
			ret = -errno;
			break;
		}
		offset += ret;
	}

	sprintf(rsp_buf, "\r\n#XSEND: %d\r\n", offset);
	rsp_send(rsp_buf, strlen(rsp_buf));

	if (ret >= 0) {
		return 0;
	} else {
		return ret;
	}
}

static int do_recv(uint16_t length)
{
	int ret;
	int sockfd = sock.fd;

	/* For TCP/TLS Server, receive from imcoming socket */
	if (sock.type == SOCK_STREAM && sock.role == AT_SOCKET_ROLE_SERVER) {
		if (sock.fd_peer != INVALID_SOCKET) {
			sockfd = sock.fd_peer;
		} else {
			LOG_ERR("No remote connection");
			return -EINVAL;
		}
	}

	ret = recv(sockfd, (void *)rx_data, length, 0);
	if (ret < 0) {
		LOG_WRN("recv() error: %d", -errno);
		if (errno != EAGAIN && errno != ETIMEDOUT) {
			do_socket_close(-errno);
		} else {
			sprintf(rsp_buf, "\r\n#XSOCKET: %d\r\n", -errno);
			rsp_send(rsp_buf, strlen(rsp_buf));
		}
		return -errno;
	}
	/**
	 * When a stream socket peer has performed an orderly shutdown,
	 * the return value will be 0 (the traditional "end-of-file")
	 * The value 0 may also be returned if the requested number of
	 * bytes to receive from a stream socket was 0
	 * In both cases, treat as normal shutdown by remote
	 */
	if (ret == 0) {
		LOG_WRN("recv() return 0");
	} else {
		rsp_send(rx_data, ret);
		sprintf(rsp_buf, "\r\n#XRECV: %d,%d\r\n", DATATYPE_PLAINTEXT, ret);
		rsp_send(rsp_buf, strlen(rsp_buf));
		ret = 0;
	}

	return ret;
}

static int do_sendto(const char *url, uint16_t port, const uint8_t *data, int datalen)
{
	int ret = 0;
	struct addrinfo *res;
	uint32_t offset = 0;

	LOG_DBG("sendto %s:%d", log_strdup(url), port);

	ret = getaddrinfo(url, NULL, NULL, &res);
	if (ret) {
		LOG_ERR("getaddrinfo() error: %s", log_strdup(gai_strerror(ret)));
		return -EAGAIN;
	}

	/* NOTE use first resolved address as target */
	if ((sock.family == AF_INET && res->ai_family != AF_INET) ||
	    (sock.family == AF_INET6 && res->ai_family != AF_INET6)) {
		LOG_ERR("Address family mismatch");
		freeaddrinfo(res);
		return -EINVAL;
	}

	if (sock.sec_tag != INVALID_SEC_TAG) {
		if (sock.hostname_verify) {
			ret = setsockopt(sock.fd, SOL_TLS, TLS_HOSTNAME, url, strlen(url));
			if (ret < 0) {
				LOG_ERR("Failed to set TLS_HOSTNAME (%d)", -errno);
				do_socket_close(-errno);
				return -errno;
			}
#ifdef SOCKET_FUTURE_FEATURE
/* Due to bug report NCSIDB-497, cannot explicitly clear TLS_HOSTNAME yet*/
		} else {
			ret = setsockopt(sock.fd, SOL_TLS, TLS_HOSTNAME, NULL, 0);
			if (ret < 0) {
				LOG_ERR("Failed to clear TLS_HOSTNAME (%d)", -errno);
				do_socket_close(-errno);
				return -errno;
			}
#endif
		}
	}

	while (offset < datalen) {
		if (res->ai_family == AF_INET) {
			struct sockaddr_in *peer = (struct sockaddr_in *)res->ai_addr;

			peer->sin_port = htons(port);
			ret = sendto(sock.fd, data + offset, datalen - offset, 0,
				(struct sockaddr *)peer, sizeof(struct sockaddr_in));
		} else {
			struct sockaddr_in6 *peer = (struct sockaddr_in6 *)res->ai_addr;

			peer->sin6_port = htons(port);
			ret = sendto(sock.fd, data + offset, datalen - offset, 0,
				(struct sockaddr *)peer, sizeof(struct sockaddr_in6));
		}
		if (ret <= 0) {
			LOG_ERR("sendto() failed: %d", -errno);
			if (errno != EAGAIN && errno != ETIMEDOUT) {
				do_socket_close(-errno);
			} else {
				sprintf(rsp_buf, "\r\n#XSOCKET: %d\r\n", -errno);
				rsp_send(rsp_buf, strlen(rsp_buf));
			}
			ret = -errno;
			break;
		}
		offset += ret;
	}

	sprintf(rsp_buf, "\r\n#XSENDTO: %d\r\n", offset);
	rsp_send(rsp_buf, strlen(rsp_buf));

	if (ret >= 0) {
		return 0;
	} else {
		return ret;
	}
}

static int do_recvfrom(uint16_t length)
{
	int ret;
	struct sockaddr remote;
	socklen_t addrlen = sizeof(struct sockaddr);

	ret = recvfrom(sock.fd, (void *)rx_data, length, 0, &remote, &addrlen);
	if (ret < 0) {
		LOG_ERR("recvfrom() error: %d", -errno);
		if (errno != EAGAIN && errno != ETIMEDOUT) {
			do_socket_close(-errno);
		} else {
			sprintf(rsp_buf, "\r\n#XSOCKET: %d\r\n", -errno);
			rsp_send(rsp_buf, strlen(rsp_buf));
		}
		return -errno;
	}
	/**
	 * Datagram sockets in various domains permit zero-length
	 * datagrams. When such a datagram is received, the return
	 * value is 0. Treat as normal case
	 */
	if (ret == 0) {
		LOG_WRN("recvfrom() return 0");
	} else {
		char peer_addr[INET6_ADDRSTRLEN] = {0};
		int port = 0;

		if (remote.sa_family == AF_INET) {
			(void)inet_ntop(AF_INET, &((struct sockaddr_in *)&remote)->sin_addr,
			    peer_addr, sizeof(peer_addr));
			port = ((struct sockaddr_in *)&remote)->sin_port;
		} else if (remote.sa_family == AF_INET6) {
			(void)inet_ntop(AF_INET6, &((struct sockaddr_in6 *)&remote)->sin6_addr,
			    peer_addr, sizeof(peer_addr));
			port = ((struct sockaddr_in6 *)&remote)->sin6_port;
		}
		rsp_send(rx_data, ret);
		sprintf(rsp_buf, "\r\n#XRECVFROM: %d,\"%s:%d\"\r\n", ret, peer_addr, port);
		rsp_send(rsp_buf, strlen(rsp_buf));
	}

	return 0;
}

/**@brief handle AT#XSOCKET commands
 *  AT#XSOCKET=<op>[,<type>,<role>]
 *  AT#XSOCKET?
 *  AT#XSOCKET=?
 */
int handle_at_socket(enum at_cmd_type cmd_type)
{
	int err = -EINVAL;
	uint16_t op;

	switch (cmd_type) {
	case AT_CMD_TYPE_SET_COMMAND:
		err = at_params_unsigned_short_get(&at_param_list, 1, &op);
		if (err) {
			return err;
		}
		if (op == AT_SOCKET_OPEN_IPV4 || op == AT_SOCKET_OPEN_IPV6) {
			err = at_params_unsigned_short_get(&at_param_list, 2, &sock.type);
			if (err) {
				return err;
			}
			err = at_params_unsigned_short_get(&at_param_list, 3, &sock.role);
			if (err) {
				return err;
			}
			if (sock.fd > 0) {
				LOG_WRN("Socket is already opened");
				return -EINVAL;
			}

			sock.family = (op == AT_SOCKET_OPEN_IPV4) ? AF_INET : AF_INET6;
			err = do_socket_open();
		} else if (op == AT_SOCKET_CLOSE) {
			if (sock.fd < 0) {
				LOG_WRN("Socket is not opened yet");
				return -EINVAL;
			}
				err = do_socket_close(0);
		} break;

	case AT_CMD_TYPE_READ_COMMAND:
		if (sock.fd != INVALID_SOCKET) {
			sprintf(rsp_buf, "\r\n#XSOCKET: %d,%d,%d\r\n", sock.fd,
				sock.family, sock.role);
		} else {
			sprintf(rsp_buf, "\r\n#XSOCKET: 0\r\n");
		}
		rsp_send(rsp_buf, strlen(rsp_buf));
		err = 0;
		break;

	case AT_CMD_TYPE_TEST_COMMAND:
		sprintf(rsp_buf, "\r\n#XSOCKET: (%d,%d,%d),(%d,%d),(%d,%d)",
			AT_SOCKET_CLOSE, AT_SOCKET_OPEN_IPV4, AT_SOCKET_OPEN_IPV6,
			SOCK_STREAM, SOCK_DGRAM,
			AT_SOCKET_ROLE_CLIENT, AT_SOCKET_ROLE_SERVER);
		rsp_send(rsp_buf, strlen(rsp_buf));
		sprintf(rsp_buf, "\r\n,<sec-tag>\r\n");
		rsp_send(rsp_buf, strlen(rsp_buf));
		err = 0;
		break;

	default:
		break;
	}

	return err;
}

/**@brief handle AT#XSOCKET commands
 *  AT#XSSOCKET=<op>[,<type>,<role>,sec_tag>[,<peer_verify>[,<hostname_verify>]]]
 *  AT#XSSOCKET?
 *  AT#XSSOCKET=?
 */
int handle_at_secure_socket(enum at_cmd_type cmd_type)
{
	int err = -EINVAL;
	uint16_t op;

	switch (cmd_type) {
	case AT_CMD_TYPE_SET_COMMAND:
		err = at_params_unsigned_short_get(&at_param_list, 1, &op);
		if (err) {
			return err;
		}
		if (op == AT_SOCKET_OPEN_IPV4 || op == AT_SOCKET_OPEN_IPV6) {
			/** Peer verification level for TLS connection.
			 *    - 0 - none
			 *    - 1 - optional
			 *    - 2 - required
			 * If not set, socket will use defaults (none for servers,
			 * required for clients)
			 */
			uint16_t peer_verify;

			err = at_params_unsigned_short_get(&at_param_list, 2, &sock.type);
			if (err) {
				return err;
			}
			err = at_params_unsigned_short_get(&at_param_list, 3, &sock.role);
			if (err) {
				return err;
			}
			if (sock.role == AT_SOCKET_ROLE_SERVER) {
				peer_verify = TLS_PEER_VERIFY_NONE;
			} else if (sock.role == AT_SOCKET_ROLE_CLIENT) {
				peer_verify = TLS_PEER_VERIFY_REQUIRED;
			} else {
				return -EINVAL;
			}
			err = at_params_unsigned_int_get(&at_param_list, 4, &sock.sec_tag);
			if (err) {
				return err;
			}
			if (at_params_valid_count_get(&at_param_list) > 5) {
				err = at_params_unsigned_short_get(&at_param_list, 5,
								   &peer_verify);
				if (err) {
					return err;
				}
			}
			/** Set hostname. It accepts a string containing the hostname (may be NULL
			 * to disable hostname verification). By default, hostname check is
			 * enforced for TLS clients but we disable it
			 */
			sock.hostname_verify = 0;
			if (at_params_valid_count_get(&at_param_list) > 6) {
				err = at_params_unsigned_short_get(&at_param_list, 6,
							&sock.hostname_verify);
				if (err) {
					return err;
				}
			}

			sock.family = (op == AT_SOCKET_OPEN_IPV4) ? AF_INET : AF_INET6;
			err = do_secure_socket_open(peer_verify);
		} else if (op == AT_SOCKET_CLOSE) {
			if (sock.fd < 0) {
				LOG_WRN("Socket is not opened yet");
				return -EINVAL;
			}
			err = do_socket_close(0);
		} break;

	case AT_CMD_TYPE_READ_COMMAND:
		if (sock.fd != INVALID_SOCKET) {
			sprintf(rsp_buf, "\r\n#XSSOCKET: %d,%d,%d\r\n", sock.fd,
				sock.family, sock.role);
		} else {
			sprintf(rsp_buf, "\r\n#XSSOCKET: 0\r\n");
		}
		rsp_send(rsp_buf, strlen(rsp_buf));
		err = 0;
		break;

	case AT_CMD_TYPE_TEST_COMMAND:
		sprintf(rsp_buf, "\r\n#XSSOCKET: (%d,%d,%d),(%d,%d),(%d,%d)",
			AT_SOCKET_CLOSE, AT_SOCKET_OPEN_IPV4, AT_SOCKET_OPEN_IPV6,
			SOCK_STREAM, SOCK_DGRAM,
			AT_SOCKET_ROLE_CLIENT, AT_SOCKET_ROLE_SERVER);
		rsp_send(rsp_buf, strlen(rsp_buf));
		sprintf(rsp_buf, "\r\n,<sec-tag>,<peer_verify>,<hostname_verify>\r\n");
		rsp_send(rsp_buf, strlen(rsp_buf));
		err = 0;
		break;

	default:
		break;
	}

	return err;
}


/**@brief handle AT#XSOCKETOPT commands
 *  AT#XSOCKETOPT=<op>,<name>[,<value>]
 *  AT#XSOCKETOPT? READ command not supported
 *  AT#XSOCKETOPT=? TEST command not supported
 */
int handle_at_socketopt(enum at_cmd_type cmd_type)
{
	int err = -EINVAL;
	uint16_t op;
	uint16_t name;
	enum at_param_type type = AT_PARAM_TYPE_NUM_INT;

	switch (cmd_type) {
	case AT_CMD_TYPE_SET_COMMAND:
		if (sock.fd < 0) {
			LOG_ERR("Socket not opened yet");
			return err;
		}
		err = at_params_unsigned_short_get(&at_param_list, 1, &op);
		if (err) {
			return err;
		}
		err = at_params_unsigned_short_get(&at_param_list, 2, &name);
		if (err) {
			return err;
		}
		if (op == AT_SOCKETOPT_SET) {
			int value_int  = 0;
			char value_str[IFNAMSIZ] = {0};
			int size = IFNAMSIZ;

			if (at_params_valid_count_get(&at_param_list) > 3) {
				type = at_params_type_get(&at_param_list, 3);
				if (type == AT_PARAM_TYPE_NUM_INT) {
					err = at_params_int_get(&at_param_list, 3, &value_int);
					if (err) {
						return err;
					}
				} else if (type == AT_PARAM_TYPE_STRING) {
					err = util_string_get(&at_param_list, 3, value_str, &size);
					if (err) {
						return err;
					}
				} else {
					return -EINVAL;
				}
			}
			if (type == AT_PARAM_TYPE_NUM_INT) {
				err = do_socketopt_set_int(name, value_int);
			} else if (type == AT_PARAM_TYPE_STRING) {
				err = do_socketopt_set_str(name, value_str);
			}
		} else if (op == AT_SOCKETOPT_GET) {
			err = do_socketopt_get(name);
		} break;

	case AT_CMD_TYPE_TEST_COMMAND:
		sprintf(rsp_buf, "\r\n#XSOCKETOPT: (%d,%d),<name>,<value>\r\n",
			AT_SOCKETOPT_GET, AT_SOCKETOPT_SET);
		rsp_send(rsp_buf, strlen(rsp_buf));
		err = 0;
		break;

	default:
		break;
	}

	return err;
}

/**@brief handle AT#XBIND commands
 *  AT#XBIND=<port>
 *  AT#XBIND?
 *  AT#XBIND=? TEST command not supported
 */
int handle_at_bind(enum at_cmd_type cmd_type)
{
	int err = -EINVAL;
	uint16_t port;

	if (sock.fd < 0) {
		LOG_ERR("Socket not opened yet");
		return err;
	}

	switch (cmd_type) {
	case AT_CMD_TYPE_SET_COMMAND:
		err = at_params_unsigned_short_get(&at_param_list, 1, &port);
		if (err < 0) {
			return err;
		}
		err = do_bind((uint16_t)port);
		break;

	default:
		break;
	}

	return err;
}

/**@brief handle AT#XCONNECT commands
 *  AT#XCONNECT=<url>,<port>
 *  AT#XCONNECT?
 *  AT#XCONNECT=? TEST command not supported
 */
int handle_at_connect(enum at_cmd_type cmd_type)
{
	int err = -EINVAL;
	char url[TCPIP_MAX_URL];
	int size = TCPIP_MAX_URL;
	uint16_t port;

	if (sock.fd < 0) {
		LOG_ERR("Socket not opened yet");
		return err;
	}
	if (sock.role != AT_SOCKET_ROLE_CLIENT) {
		LOG_ERR("Invalid role");
		return err;
	}

	switch (cmd_type) {
	case AT_CMD_TYPE_SET_COMMAND:
		err = util_string_get(&at_param_list, 1, url, &size);
		if (err) {
			return err;
		}
		err = at_params_unsigned_short_get(&at_param_list, 2, &port);
		if (err) {
			return err;
		}
		err = do_connect(url, (uint16_t)port);
		break;

	default:
		break;
	}

	return err;
}

/**@brief handle AT#XLISTEN commands
 *  AT#XLISTEN
 *  AT#XLISTEN? READ command not supported
 *  AT#XLISTEN=? TEST command not supported
 */
int handle_at_listen(enum at_cmd_type cmd_type)
{
	int err = -EINVAL;

	if (sock.fd < 0) {
		LOG_ERR("Socket not opened yet");
		return err;
	}
	if (sock.role != AT_SOCKET_ROLE_SERVER) {
		LOG_ERR("Invalid role");
		return err;
	}

	switch (cmd_type) {
	case AT_CMD_TYPE_SET_COMMAND:
		err = do_listen();
		break;

	default:
		break;
	}

	return err;
}

/**@brief handle AT#XACCEPT commands
 *  AT#XACCEPT
 *  AT#XACCEPT? READ command not supported
 *  AT#XACCEPT=? TEST command not supported
 */
int handle_at_accept(enum at_cmd_type cmd_type)
{
	int err = -EINVAL;

	if (sock.fd < 0) {
		LOG_ERR("Socket not opened yet");
		return err;
	}
	if (sock.role != AT_SOCKET_ROLE_SERVER) {
		LOG_ERR("Invalid role");
		return err;
	}

	switch (cmd_type) {
	case AT_CMD_TYPE_SET_COMMAND:
		err = do_accept();
		break;

	case AT_CMD_TYPE_READ_COMMAND:
		if (sock.fd_peer != INVALID_SOCKET) {
			sprintf(rsp_buf, "\r\n#XTCPACCEPT: %d\r\n",
				sock.fd_peer);
		} else {
			sprintf(rsp_buf, "\r\n#XTCPACCEPT: 0\r\n");
		}
		rsp_send(rsp_buf, strlen(rsp_buf));
		err = 0;
		break;

	default:
		break;
	}

	return err;
}

/**@brief handle AT#XSEND commands
 *  AT#XSEND=<data>
 *  AT#XSEND? READ command not supported
 *  AT#XSEND=? TEST command not supported
 */
int handle_at_send(enum at_cmd_type cmd_type)
{
	int err = -EINVAL;
	char data[NET_IPV4_MTU];
	int size = NET_IPV4_MTU;

	switch (cmd_type) {
	case AT_CMD_TYPE_SET_COMMAND:
		err = util_string_get(&at_param_list, 1, data, &size);
		if (err) {
			return err;
		}
		err = do_send(data, size);
		break;

	default:
		break;
	}

	return err;
}

/**@brief handle AT#XRECV commands
 *  AT#XRECV[=<length>]
 *  AT#XRECV? READ command not supported
 *  AT#XRECV=? TEST command not supported
 */
int handle_at_recv(enum at_cmd_type cmd_type)
{
	int err = -EINVAL;
	int16_t length;

	switch (cmd_type) {
	case AT_CMD_TYPE_SET_COMMAND:
		err = at_params_unsigned_short_get(&at_param_list, 1, &length);
		if (err) {
			length = CONFIG_SLM_SOCKET_RX_MAX;
		}
		err = do_recv(length);
		break;

	default:
		break;
	}

	return err;
}

/**@brief handle AT#XSENDTO commands
 *  AT#XSENDTO=<url>,<port>,<datatype>,<data>
 *  AT#XSENDTO? READ command not supported
 *  AT#XSENDTO=? TEST command not supported
 */
int handle_at_sendto(enum at_cmd_type cmd_type)
{
	int err = -EINVAL;
	char url[TCPIP_MAX_URL];
	int size = TCPIP_MAX_URL;
	uint16_t port;
	char data[NET_IPV4_MTU];

	if (sock.fd < 0) {
		LOG_ERR("Socket not opened yet");
		return err;
	}

	switch (cmd_type) {
	case AT_CMD_TYPE_SET_COMMAND:
		err = util_string_get(&at_param_list, 1, url, &size);
		if (err) {
			return err;
		}
		err = at_params_unsigned_short_get(&at_param_list, 2, &port);
		if (err) {
			return err;
		}
		size = NET_IPV4_MTU;
		err = util_string_get(&at_param_list, 3, data, &size);
		if (err) {
			return err;
		}
		err = do_sendto(url, port, data, size);
		break;

	default:
		break;
	}

	return err;
}

/**@brief handle AT#XRECVFROM commands
 *  AT#XRECVFROM[=<length>]
 *  AT#XRECVFROM? READ command not supported
 *  AT#XRECVFROM=? TEST command not supported
 */
int handle_at_recvfrom(enum at_cmd_type cmd_type)
{
	int err = -EINVAL;
	uint16_t length;

	if (sock.fd < 0) {
		LOG_ERR("Socket not opened yet");
		return err;
	}

	switch (cmd_type) {
	case AT_CMD_TYPE_SET_COMMAND:
		err = at_params_unsigned_short_get(&at_param_list, 1, &length);
		if (err) {
			length = CONFIG_SLM_SOCKET_RX_MAX;
		}
		err = do_recvfrom(length);
		break;

	default:
		break;
	}

	return err;
}

/**@brief handle AT#XGETADDRINFO commands
 *  AT#XGETADDRINFO=<url>
 *  AT#XGETADDRINFO? READ command not supported
 *  AT#XGETADDRINFO=? TEST command not supported
 */
int handle_at_getaddrinfo(enum at_cmd_type cmd_type)
{
	int err = -EINVAL;
	char hostname[NI_MAXHOST];
	char host[TCPIP_MAX_URL];
	int size = TCPIP_MAX_URL;
	struct addrinfo *result;
	struct addrinfo *res;

	switch (cmd_type) {
	case AT_CMD_TYPE_SET_COMMAND:
		err = util_string_get(&at_param_list, 1, host, &size);
		if (err) {
			return err;
		}
		err = getaddrinfo(host, NULL, NULL, &result);
		if (err) {
			sprintf(rsp_buf, "\r\n#XGETADDRINFO: \"%s\"\r\n", gai_strerror(err));
			rsp_send(rsp_buf, strlen(rsp_buf));
			return err;
		} else if (result == NULL) {
			sprintf(rsp_buf, "\r\n#XGETADDRINFO: \"not found\"\r\n");
			rsp_send(rsp_buf, strlen(rsp_buf));
			return -ENOENT;
		}

		sprintf(rsp_buf, "\r\n#XGETADDRINFO: \"");
		/* loop over all returned results and do inverse lookup */
		for (res = result; res != NULL; res = res->ai_next) {
			if (res->ai_family == AF_INET) {
				struct sockaddr_in *host = (struct sockaddr_in *)result->ai_addr;

				inet_ntop(AF_INET, &host->sin_addr, hostname, sizeof(hostname));
			} else if (res->ai_family == AF_INET6) {
				struct sockaddr_in6 *host = (struct sockaddr_in6 *)result->ai_addr;

				inet_ntop(AF_INET6, &host->sin6_addr, hostname, sizeof(hostname));
			} else {
				continue;
			}

			strcat(rsp_buf, hostname);
			if (res->ai_next) {
				strcat(rsp_buf, " ");
			}
		}
		strcat(rsp_buf, "\"\r\n");
		rsp_send(rsp_buf, strlen(rsp_buf));
		freeaddrinfo(result);
		break;

	default:
		break;
	}

	return err;
}

/**@brief API to initialize TCP/IP AT commands handler
 */
int slm_at_tcpip_init(void)
{
	sock.family  = AF_UNSPEC;
	sock.sec_tag = INVALID_SEC_TAG;
	sock.role    = AT_SOCKET_ROLE_CLIENT;
	sock.fd      = INVALID_SOCKET;
	sock.fd_peer = INVALID_SOCKET;

	return 0;
}

/**@brief API to uninitialize TCP/IP AT commands handler
 */
int slm_at_tcpip_uninit(void)
{
	return do_socket_close(0);
}
