/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */
#include <logging/log.h>

#include <zephyr.h>
#include <stdio.h>
#include <string.h>
#include <net/lwm2m.h>
#include <modem_info.h>
#include "elp_connect.h"
#include "elp_util.h"
#include "elp_lwm2m_service.h"

LOG_MODULE_REGISTER(lwm2m, CONFIG_ELP_LOG_LEVEL);

#define SERVER_ADDR		CONFIG_ELP_LWM2M_SERVER
#define CLIENT_MANUFACTURER	"Nordic Semiconductor"
#define CLIENT_MODEL_NUMBER	"OMA-LWM2M Sample Client"
#define CLIENT_SERIAL_NUMBER	"345000123"
#define CLIENT_FIRMWARE_VER	"1.0"
#define CLIENT_DEVICE_TYPE	"OMA-LWM2M Client"
#define CLIENT_HW_VER		"1.0.1"

#define MAX_URI_LENGTH		16
#define MAX_READ_LENGTH		(CONFIG_LWM2M_ADC_DATA_SIZE + 4)

enum lwm2m_int_type {
	LWM2M_INT_TYPE_BOOLEAN,
	LWM2M_INT_TYPE_UINT8,
	LWM2M_INT_TYPE_UINT16,
	LWM2M_INT_TYPE_UINT32,
	LWM2M_INT_TYPE_UINT64,
	LWM2M_INT_TYPE_INT8,
	LWM2M_INT_TYPE_INT16,
	LWM2M_INT_TYPE_INT32,
	LWM2M_INT_TYPE_INT64
};

enum lwm2m_float_type {
	LWM2M_FLOAD_TYPE_32,
	LWM2M_FLOAD_TYPE_64
};

static u8_t bat_idx = LWM2M_DEVICE_PWR_SRC_TYPE_BAT_INT;
static int bat_mv = 3800;
static int bat_ma = 125;
static u8_t usb_idx = LWM2M_DEVICE_PWR_SRC_TYPE_USB;
static int usb_mv = 5000;
static int usb_ma = 900;
static u8_t bat_level = 95;
static u8_t bat_status = LWM2M_DEVICE_BATTERY_STATUS_CHARGING;
static int mem_free = 15;
static int mem_total = 25;

static struct lwm2m_ctx client;

#if defined(CONFIG_LWM2M_DTLS_SUPPORT)
#define TLS_TAG			1

/* "000102030405060708090a0b0c0d0e0f" */
static unsigned char client_psk[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
};

static const char client_psk_id[] = "Client_identity";
#endif /* CONFIG_LWM2M_DTLS_SUPPORT */

#if defined(CONFIG_LWM2M_FIRMWARE_UPDATE_OBJ_SUPPORT)
static u8_t firmware_buf[64];
#endif

#define IMEI_LEN		15
#define ENDPOINT_NAME_LEN	(IMEI_LEN + 3)

static u8_t endpoint_name[ENDPOINT_NAME_LEN];
static char path[MAX_URI_LENGTH];

/* global variable defined in different files */
extern struct modem_param_info modem_param;

static int adc_create_cb(u16_t obj_inst_id)
{
	LOG_INF("ADC instance %d created", obj_inst_id);

	return 0;
}

static int adc_data_post_notify_cb(u16_t obj_inst_id,
				 u16_t res_id, int result)
{
	char buf[sizeof(int)];

	LOG_INF("ADC NOTIFY ins: %d, res: %d, result: %d",
		obj_inst_id, res_id, result);

	memcpy(buf, &result, sizeof(result));
	(void)inter_connect_notify(NOT_TYPE_LWM2M_NOTIFY_RESULT, buf, sizeof(result));

	return 0;
}

static int adc_data_post_write_cb(u16_t obj_inst_id,
				 u16_t res_id, u16_t res_inst_id,
				 u8_t *data, u16_t data_len,
				 bool last_block, size_t total_size)
{
	char buf[CONFIG_LWM2M_ADC_DATA_SIZE];

	ARG_UNUSED(obj_inst_id);
	ARG_UNUSED(res_id);
	ARG_UNUSED(res_inst_id);
	ARG_UNUSED(last_block);
	ARG_UNUSED(total_size);

	if (data_len > CONFIG_LWM2M_ADC_DATA_SIZE) {
		LOG_ERR("WRITE sizeover (%d)", data_len);
		return -EINVAL;
	}

	LOG_HEXDUMP_DBG(data, data_len, "ADC-WR");

	memcpy(buf, data, data_len);
	(void)inter_connect_notify(NOT_TYPE_LWM2M_OBJECT, buf, data_len);
	return 0;
}

static int device_reboot_cb(u16_t obj_inst_id)
{
	LOG_INF("DEVICE: REBOOT");
	/* Add an error for testing */
	lwm2m_device_add_err(LWM2M_DEVICE_ERROR_LOW_POWER);
	/* Change the battery voltage for testing */
	lwm2m_engine_set_s32("3/0/7/0", (bat_mv - 1));

	return 0;
}

static int device_factory_default_cb(u16_t obj_inst_id)
{
	LOG_INF("DEVICE: FACTORY DEFAULT");
	/* Add an error for testing */
	lwm2m_device_add_err(LWM2M_DEVICE_ERROR_GPS_FAILURE);
	/* Change the USB current for testing */
	lwm2m_engine_set_s32("3/0/8/1", (usb_ma - 1));
	return 0;
}

#if defined(CONFIG_LWM2M_FIRMWARE_UPDATE_PULL_SUPPORT)
static int firmware_update_cb(u16_t obj_inst_id)
{
	LOG_DBG("UPDATE");

	/* TODO: kick off update process */

	/* If success, set the update result as RESULT_SUCCESS.
	 * In reality, it should be set at function lwm2m_setup()
	 */
	lwm2m_engine_set_u8("5/0/3", STATE_IDLE);
	lwm2m_engine_set_u8("5/0/5", RESULT_SUCCESS);
	return 0;
}
#endif

#if defined(CONFIG_LWM2M_FIRMWARE_UPDATE_OBJ_SUPPORT)
static void *firmware_get_buf(u16_t obj_inst_id, size_t *data_len)
{
	*data_len = sizeof(firmware_buf);
	return firmware_buf;
}

static int firmware_block_received_cb(u16_t obj_inst_id,
				u8_t *data, u16_t data_len,
				bool last_block, size_t total_size)
{
	LOG_INF("FIRMWARE: BLOCK RECEIVED: len:%u last_block:%d",
		data_len, last_block);
	return 0;
}
#endif

static int lwm2m_setup(u16_t lifetime)
{
	int ret;
	char *server_url;
	u16_t server_url_len;
	u8_t server_url_flags;
	static bool object_instances_created;

	/* setup SECURITY object */

	/* Server URL */
	LOG_INF("Server URL: %s", log_strdup(SERVER_ADDR));
	ret = lwm2m_engine_get_res_data("0/0/0",
				(void **)&server_url, &server_url_len,
				&server_url_flags);
	if (ret < 0) {
		return ret;
	}

#if CONFIG_LWM2M_RD_CLIENT_SUPPORT_BOOTSTRAP
	/* Mark 1st instance of security object as a bootstrap server */
	lwm2m_engine_set_u8("0/0/1", 1);

	if (!object_instances_created) {
		/* Create 2nd instance of server and security objects, */
		/* needed for bootstrap process */
		lwm2m_engine_create_obj_inst("0/1");
		lwm2m_engine_create_obj_inst("1/1");
	}
#endif

	snprintk(server_url, server_url_len, "coap%s//%s%s%s",
		 IS_ENABLED(CONFIG_LWM2M_DTLS_SUPPORT) ? "s:" : ":",
		 strchr(SERVER_ADDR, ':') ? "[" : "", SERVER_ADDR,
		 strchr(SERVER_ADDR, ':') ? "]" : "");

	/* Security Mode */
	lwm2m_engine_set_u8("0/0/2",
		IS_ENABLED(CONFIG_LWM2M_DTLS_SUPPORT) ? 0 : 3);
#if defined(CONFIG_LWM2M_DTLS_SUPPORT)
	lwm2m_engine_set_string("0/0/3", (char *)client_psk_id);
	lwm2m_engine_set_opaque("0/0/5",
				(void *)client_psk, sizeof(client_psk));
#endif /* CONFIG_LWM2M_DTLS_SUPPORT */

	/* setup SERVER object */
	lwm2m_engine_set_u32("1/0/1", lifetime);
#if CONFIG_LWM2M_RD_CLIENT_SUPPORT_BOOTSTRAP
	lwm2m_engine_set_u32("1/1/1", lifetime);
#endif

	/* setup DEVICE object */
	lwm2m_engine_set_res_data("3/0/0", CLIENT_MANUFACTURER,
				sizeof(CLIENT_MANUFACTURER),
				LWM2M_RES_DATA_FLAG_RO);
	lwm2m_engine_set_res_data("3/0/1", CLIENT_MODEL_NUMBER,
				sizeof(CLIENT_MODEL_NUMBER),
				LWM2M_RES_DATA_FLAG_RO);
	lwm2m_engine_set_res_data("3/0/2", CLIENT_SERIAL_NUMBER,
				sizeof(CLIENT_SERIAL_NUMBER),
				LWM2M_RES_DATA_FLAG_RO);
	lwm2m_engine_set_res_data("3/0/3", CLIENT_FIRMWARE_VER,
				sizeof(CLIENT_FIRMWARE_VER),
				LWM2M_RES_DATA_FLAG_RO);
	lwm2m_engine_register_exec_callback("3/0/4", device_reboot_cb);
	lwm2m_engine_register_exec_callback("3/0/5", device_factory_default_cb);
	lwm2m_engine_set_res_data("3/0/9", &bat_level, sizeof(bat_level), 0);
	lwm2m_engine_set_res_data("3/0/10", &mem_free, sizeof(mem_free), 0);
	lwm2m_engine_set_res_data("3/0/17", CLIENT_DEVICE_TYPE,
				sizeof(CLIENT_DEVICE_TYPE),
				LWM2M_RES_DATA_FLAG_RO);
	lwm2m_engine_set_res_data("3/0/18", CLIENT_HW_VER,
				sizeof(CLIENT_HW_VER),
				LWM2M_RES_DATA_FLAG_RO);
	lwm2m_engine_set_res_data("3/0/20", &bat_status, sizeof(bat_status), 0);
	lwm2m_engine_set_res_data("3/0/21", &mem_total, sizeof(mem_total), 0);

	if (!object_instances_created) {
		/* add power source resource instances */
		lwm2m_engine_create_res_inst("3/0/6/0");
		lwm2m_engine_set_res_data("3/0/6/0", &bat_idx,
					sizeof(bat_idx), 0);
		lwm2m_engine_create_res_inst("3/0/7/0");
		lwm2m_engine_set_res_data("3/0/7/0", &bat_mv,
					sizeof(bat_mv), 0);
		lwm2m_engine_create_res_inst("3/0/8/0");
		lwm2m_engine_set_res_data("3/0/8/0", &bat_ma,
					sizeof(bat_ma), 0);
		lwm2m_engine_create_res_inst("3/0/6/1");
		lwm2m_engine_set_res_data("3/0/6/1", &usb_idx,
					sizeof(usb_idx), 0);
		lwm2m_engine_create_res_inst("3/0/7/1");
		lwm2m_engine_set_res_data("3/0/7/1", &usb_mv,
					sizeof(usb_mv), 0);
		lwm2m_engine_create_res_inst("3/0/8/1");
		lwm2m_engine_set_res_data("3/0/8/1", &usb_ma,
					sizeof(usb_ma), 0);
	}

	/* setup FIRMWARE object */
#if defined(CONFIG_LWM2M_FIRMWARE_UPDATE_OBJ_SUPPORT)
	/* setup data buffer for block-wise transfer */
	lwm2m_engine_register_pre_write_callback("5/0/0", firmware_get_buf);
	lwm2m_firmware_set_write_cb(firmware_block_received_cb);
#endif
#if defined(CONFIG_LWM2M_FIRMWARE_UPDATE_PULL_SUPPORT)
	lwm2m_firmware_set_update_cb(firmware_update_cb);
#endif

	if (!object_instances_created) {
	        /* setup BinaryAppDataContainer Object */
		lwm2m_engine_register_create_callback(19, adc_create_cb);
		lwm2m_engine_create_obj_inst("19/0"); /* uplink */
		lwm2m_engine_register_post_notify_callback("19/0/0", adc_data_post_notify_cb);
		//lwm2m_engine_set_opaque("19/0/0",
		//	(void *)CLIENT_MANUFACTURER, sizeof(CLIENT_MANUFACTURER)-1);
		lwm2m_engine_create_obj_inst("19/1"); /* downlink */
		lwm2m_engine_register_post_write_callback("19/1/0", adc_data_post_write_cb);
	}

	/* All instances only need to create once */
	object_instances_created = true;

	return 0;
}

static void rd_client_event(struct lwm2m_ctx *client,
	enum lwm2m_rd_client_event client_event)
{
	u8_t evt;

	switch (client_event) {

	case LWM2M_RD_CLIENT_EVENT_NONE:
		/* do nothing */
		break;

	case LWM2M_RD_CLIENT_EVENT_BOOTSTRAP_REG_FAILURE:
		LOG_ERR("Bootstrap registration failure!");
		break;

	case LWM2M_RD_CLIENT_EVENT_BOOTSTRAP_REG_COMPLETE:
		LOG_INF("Bootstrap registration complete");
		break;

	case LWM2M_RD_CLIENT_EVENT_BOOTSTRAP_TRANSFER_COMPLETE:
		LOG_INF("Bootstrap transfer complete");
		break;

	case LWM2M_RD_CLIENT_EVENT_REGISTRATION_FAILURE:
		LOG_ERR("Registration failure!");
		break;

	case LWM2M_RD_CLIENT_EVENT_REGISTRATION_COMPLETE:
		LOG_INF("Registration complete");
		break;

	case LWM2M_RD_CLIENT_EVENT_REG_UPDATE_FAILURE:
		LOG_ERR("Registration update failure!");
		break;

	case LWM2M_RD_CLIENT_EVENT_REG_UPDATE_COMPLETE:
		LOG_INF("Registration update complete");
		break;

	case LWM2M_RD_CLIENT_EVENT_DEREGISTER_FAILURE:
		LOG_ERR("Deregister failure!");
		break;

	case LWM2M_RD_CLIENT_EVENT_DISCONNECT:
		LOG_INF("Disconnected");
		break;

	default:
		LOG_WRN("Unknown event");
		break;
	}

	evt = client_event;
	(void)inter_connect_notify(NOT_TYPE_LWM2M_RD, &evt, 1);
}

int do_lwm2m_connect(const u8_t *param, u8_t length)
{
	int ret;
	u16_t lifetime = 0;

	if (param != NULL && length > 1) {
		memcpy(&lifetime, param, 2);
	}
	if (lifetime < 15 || lifetime > 65535){
		LOG_WRN("Invalid lifetime (%d)", lifetime);
		lifetime = CONFIG_LWM2M_ENGINE_DEFAULT_LIFETIME;
	}

	ret = lwm2m_setup(lifetime);
	if (ret < 0) {
		LOG_ERR("Cannot setup LWM2M fields (%d)", ret);
		return ret;
	}

	(void)memset(&client, 0x0, sizeof(client));

	ret = modem_info_params_get(&modem_param);
	if ( ret < 0) {
		LOG_ERR("Unable to obtain modem parameters (%d)", ret);
		return -1;
	}

	/* use IMEI as unique endpoint name */
	memcpy(endpoint_name, modem_param.device.imei.value_string,
		strlen(modem_param.device.imei.value_string));
	//snprintf(endpoint_name, sizeof(endpoint_name), "i: %s",
	//	modem_param.device.imei.value_string);

	/* client.sec_obj_inst is 0 as a starting point */
	LOG_INF("Start client");
	lwm2m_rd_client_start(&client, endpoint_name, rd_client_event);
	return 0;
}

int do_lwm2m_disconnect(void)
{
	LOG_INF("Stop client");
	lwm2m_rd_client_stop(&client, rd_client_event);
	return 0;
}

int do_lwm2m_set_path(const u8_t *param, u8_t length)
{
	if (length < MAX_URI_LENGTH) {
		memcpy(path, param, length);
		path[length] = '\0';
		LOG_INF("Path set (%s)", log_strdup(path));
		return 0;
	} else {
		LOG_ERR("Path too long (%d)", length);
		return -EINVAL;
	}
}

int do_lwm2m_read_int(const u8_t *param, u8_t length, u8_t *output)
{
	u8_t int_type = *param;
	int ret = -EINVAL;

	switch (int_type) {
	case LWM2M_INT_TYPE_BOOLEAN: {
		bool value;

		ret = lwm2m_engine_get_bool(path, &value);
		if (ret == 0) {
			*output = value;
			ret = 1;
		}
	} break;
	case LWM2M_INT_TYPE_UINT8: {
		u8_t value;

		ret = lwm2m_engine_get_u8(path, &value);
		if (ret == 0) {
			*output = value;
			ret = 1;
		}
	} break;
	case LWM2M_INT_TYPE_UINT16: {
		u16_t value;

		ret = lwm2m_engine_get_u16(path, &value);
		if (ret == 0) {
			memcpy(output, &value, 2);
			ret = 2;
		}
	} break;
	case LWM2M_INT_TYPE_UINT32: {
		u32_t value;

		ret = lwm2m_engine_get_u32(path, &value);
		if (ret == 0) {
			memcpy(output, &value, 4);
			ret = 4;
		}
	} break;
	case LWM2M_INT_TYPE_UINT64: {
		u64_t value;

		ret = lwm2m_engine_get_u64(path, &value);
		if (ret == 0) {
			memcpy(output, &value, 8);
			ret = 8;
		}
	} break;
	case LWM2M_INT_TYPE_INT8: {
		s8_t value;

		ret = lwm2m_engine_get_s8(path, &value);
		if (ret == 0) {
			*output = value;
			ret = 1;
		}
	} break;
	case LWM2M_INT_TYPE_INT16: {
		s16_t value;

		ret = lwm2m_engine_get_s16(path, &value);
		if (ret == 0) {
			memcpy(output, &value, 2);
			ret = 2;
		}
	} break;
	case LWM2M_INT_TYPE_INT32: {
		s32_t value;

		ret = lwm2m_engine_get_s32(path, &value);
		if (ret == 0) {
			memcpy(output, &value, 4);
			ret = 4;
		}
	} break;
	case LWM2M_INT_TYPE_INT64: {
		s64_t value;

		ret = lwm2m_engine_get_s64(path, &value);
		if (ret == 0) {
			memcpy(output, &value, 8);
			ret = 8;
		}
	} break;
	default:
		break;
	}

	return ret;
}

int do_lwm2m_write_int(const u8_t *param, u8_t length)
{
	u8_t int_type = *param;
	int err = -EINVAL;

	switch (int_type) {
	case LWM2M_INT_TYPE_BOOLEAN: {
		bool value = *(param+1);

		err = lwm2m_engine_set_bool(path, value);
	} break;
	case LWM2M_INT_TYPE_UINT8: {
		u8_t value = *(param+1);

		err = lwm2m_engine_set_u8(path, value);
	} break;
	case LWM2M_INT_TYPE_UINT16: {
		u16_t value;

		memcpy(&value, (param+1), 2);
		err = lwm2m_engine_set_u16(path, value);
	} break;
	case LWM2M_INT_TYPE_UINT32: {
		u32_t value;

		memcpy(&value, (param+1), 4);
		err = lwm2m_engine_set_u32(path, value);
	} break;
	case LWM2M_INT_TYPE_UINT64: {
		u64_t value;

		memcpy(&value, (param+1), 8);
		err = lwm2m_engine_set_u64(path, value);
	} break;
	case LWM2M_INT_TYPE_INT8: {
		s8_t value = *(param+1);

		memcpy(path, (param+2), length-2);
		err = lwm2m_engine_set_s8(path, value);
	} break;
	case LWM2M_INT_TYPE_INT16: {
		s16_t value;

		memcpy(&value, (param+1), 2);
		err = lwm2m_engine_set_s16(path, value);
	} break;
	case LWM2M_INT_TYPE_INT32:
	{
		s32_t value;

		memcpy(&value, (param+1), 4);
		err = lwm2m_engine_set_s32(path, value);
	} break;
	case LWM2M_INT_TYPE_INT64: {
		s64_t value;

		memcpy(&value, (param+1), 8);
		err = lwm2m_engine_set_s64(path, value);
	} break;
	default:
		break;
	}

	return err;
}

int do_lwm2m_read_float(const u8_t *param, u8_t length, u8_t *output)
{
	u8_t float_type = *param;
	int ret = -EINVAL;

	if (float_type == LWM2M_FLOAD_TYPE_32) {
		float32_value_t value;

		ret = lwm2m_engine_get_float32(path, &value);
		if (ret == 0) {
			memcpy(output, &(value.val1), 4);
			memcpy(output+4, &(value.val2), 4);
			ret = 8;
		}
	}

	if (float_type == LWM2M_FLOAD_TYPE_64) {
		float64_value_t value;

		ret = lwm2m_engine_get_float64(path, &value);
		if (ret == 0) {
			memcpy(output, &(value.val1), 8);
			memcpy(output+8, &(value.val2), 8);
			ret = 16;
		}
	}

	return ret;
}

int do_lwm2m_write_float(const u8_t *param, u8_t length)
{
	u8_t float_type = *param;
	char path[MAX_URI_LENGTH];
	int err = -EINVAL;

	if (float_type == LWM2M_FLOAD_TYPE_32) {
		float32_value_t value;

		memcpy(&(value.val1), (param+1), 4);
		memcpy(&(value.val2), (param+5), 4);
		memcpy(path, (param+9), length-9);
		err = lwm2m_engine_set_float32(path, &value);
	}

	if (float_type == LWM2M_FLOAD_TYPE_64) {
		float64_value_t value;

		memcpy(&(value.val1), (param+1), 8);
		memcpy(&(value.val2), (param+9), 8);
		memcpy(path, (param+17), length-17);
		err = lwm2m_engine_set_float64(path, &value);
	}

	return err;
}

int do_lwm2m_read_string(char *output)
{
	int ret;

	memset(output, 0x00, MAX_READ_LENGTH);
	ret = lwm2m_engine_get_string(path, (void *)output, MAX_READ_LENGTH);
	if (ret < 0) {
		return ret;
	} else {
		return strlen(output);
	}
}

int do_lwm2m_write_string(const u8_t *param, u8_t length)
{
	char data_buf[MAX_READ_LENGTH];

	memcpy(data_buf, param, length);
	data_buf[length] = '\0';
	return lwm2m_engine_set_string(path, (void *)data_buf);
}

int do_lwm2m_read_opaque(char *output)
{
	int ret;

	memset(output, 0x00, MAX_READ_LENGTH);
	ret = lwm2m_engine_get_opaque(path, (void *)output, MAX_READ_LENGTH);
	LOG_INF("Get (%s)", log_strdup(output));
	if (ret < 0) {
		return ret;
	} else {
		return strlen(output);
	}
}

int do_lwm2m_write_opaque(const u8_t *param, u8_t length)
{
	u8_t data_buf[MAX_READ_LENGTH];

	memcpy(data_buf, param, length);
	data_buf[length] = '\0';
	LOG_INF("Set (%s)", log_strdup(data_buf));
	return lwm2m_engine_set_opaque(path, (void *)data_buf, length);
}

/**@brief LwM2M control commands */
void th_lwm2m_control(u8_t cmd, const u8_t *param, u8_t length)
{
	int err = -EINVAL;
	s8_t ret;
	char read_buf[MAX_READ_LENGTH];

	switch (cmd) {
	case CMD_TYPE_LWM2M_CONNECT:
		/*param format [16-bit integer lifetime] */
		err = do_lwm2m_connect(param, length);
		break;

	case CMD_TYPE_LWM2M_DISCONNECT:
		/*param format NONE */
		err = do_lwm2m_disconnect();
		break;

	case CMD_TYPE_LWM2M_SET_PATH:
		/*param format [path(var)]*/
		err = do_lwm2m_set_path(param, length);
		break;

	case CMD_TYPE_LWM2M_READ_INT:
		/*param format [integer type(1)]]*/
		err = do_lwm2m_read_int(param, length, (read_buf+1));
		break;

	case CMD_TYPE_LWM2M_WRITE_INT:
		/*param format [integer type(1)][value(1~8)]]*/
		err = do_lwm2m_write_int(param, length);
		break;

	case CMD_TYPE_LWM2M_READ_FLOAT:
		/*param format [float type(1)]]*/
		err = do_lwm2m_read_float(param, length, (read_buf+1));
		break;

	case CMD_TYPE_LWM2M_WRITE_FLOAT:
		/*param format [float type(1)][val1(4 or 8)]]*/
		err = do_lwm2m_write_float(param, length);
		break;

	case CMD_TYPE_LWM2M_READ_STRING:
		/*param format [path(var)]*/
		err = do_lwm2m_read_string(read_buf+1);
		break;

	case CMD_TYPE_LWM2M_WRITE_STRING:
		/*param format [value(var)]]*/
		err = do_lwm2m_write_string(param, length);
		break;

	case CMD_TYPE_LWM2M_READ_OPAQUE:
		/*param format NONE*/
		err = do_lwm2m_read_opaque((read_buf+1));
		break;

	case CMD_TYPE_LWM2M_WRITE_OPAQUE:
		/*param format [value(var)]*/
		err = do_lwm2m_write_opaque(param, length);
		break;

	default:
		LOG_ERR("unknown %d", cmd);
		break;
	}

	ret = err;
	if (cmd != CMD_TYPE_LWM2M_READ_INT &&
		cmd != CMD_TYPE_LWM2M_READ_FLOAT &&
		cmd != CMD_TYPE_LWM2M_READ_STRING &&
		cmd != CMD_TYPE_LWM2M_READ_OPAQUE) {
		(void)inter_connect_send(RSP_TYPE_BASE|cmd, &ret, 1);
	} else {
	/* Read commands response */
		if (err < 0) {
			(void)inter_connect_send(RSP_TYPE_BASE|cmd, &ret, 1);
		} else {
			*read_buf = 0x00;
			(void)inter_connect_send(RSP_TYPE_BASE|cmd, read_buf,
				ret+1);
		}
	}
}

