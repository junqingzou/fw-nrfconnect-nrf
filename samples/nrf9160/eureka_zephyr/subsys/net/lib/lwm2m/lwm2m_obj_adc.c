/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_MODULE_NAME net_lwm2m_obj_adc
#define LOG_LEVEL CONFIG_LWM2M_LOG_LEVEL

#include <logging/log.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#include <stdint.h>
#include <init.h>

#include "lwm2m_object.h"
#include "lwm2m_engine.h"

/* App Data Container resource IDs */
#define APP_DATA_CONTAINER_DATA			0
#define APP_DATA_CONTAINER_DATA_PRIOTRITY	1
#define APP_DATA_CONTAINER_DATA_CREATION_TIME	2
#define APP_DATA_CONTAINER_DATA_DESCRIPTION	3
#define APP_DATA_CONTAINER_DATA_FORMAT		4
#define APP_DATA_CONTAINER_APP_ID		5

#define APP_DATA_CONTAINER_MAX_ID		6

#define MAX_INSTANCE_COUNT	CONFIG_LWM2M_ADC_INSTANCE_COUNT
#define MAX_DATA_LEN		CONFIG_LWM2M_ADC_DATA_SIZE

/*
 * Calculate resource instances as follows:
 * start with APP_DATA_CONTAINER_MAX_ID
 */
#define RESOURCE_INSTANCE_COUNT	(APP_DATA_CONTAINER_MAX_ID)

/* resource state variables */
static u8_t  adc_data[MAX_INSTANCE_COUNT][MAX_DATA_LEN];    /* Multple treat as Single for now */
static u8_t  adc_data_priority[MAX_INSTANCE_COUNT];
static u32_t adc_data_creation_time[MAX_INSTANCE_COUNT];
static u8_t  adc_data_description[MAX_INSTANCE_COUNT][32];
static u8_t  adc_data_format[MAX_INSTANCE_COUNT][32];
static u16_t adc_app_id[MAX_INSTANCE_COUNT];

static struct lwm2m_engine_obj app_data_container;
static struct lwm2m_engine_obj_field fields[] = {
	OBJ_FIELD_DATA(APP_DATA_CONTAINER_DATA, RW, OPAQUE),
	OBJ_FIELD_DATA(APP_DATA_CONTAINER_DATA_PRIOTRITY, RW_OPT, U8),
	OBJ_FIELD_DATA(APP_DATA_CONTAINER_DATA_CREATION_TIME, RW_OPT, U32),
	OBJ_FIELD_DATA(APP_DATA_CONTAINER_DATA_DESCRIPTION, RW_OPT, STRING),
	OBJ_FIELD_DATA(APP_DATA_CONTAINER_DATA_FORMAT, RW_OPT, STRING),
	OBJ_FIELD_DATA(APP_DATA_CONTAINER_APP_ID, RW_OPT, U16)
};

static struct lwm2m_engine_obj_inst inst[MAX_INSTANCE_COUNT];
static struct lwm2m_engine_res res[MAX_INSTANCE_COUNT][APP_DATA_CONTAINER_MAX_ID];
static struct lwm2m_engine_res_inst
			res_inst[MAX_INSTANCE_COUNT][RESOURCE_INSTANCE_COUNT];

static struct lwm2m_engine_obj_inst *adc_create(u16_t obj_inst_id)
{
	int index, i = 0, j = 0;

	/* Check that there is no other instance with this ID */
	for (index = 0; index < MAX_INSTANCE_COUNT; index++) {
		if (inst[index].obj && inst[index].obj_inst_id == obj_inst_id) {
			LOG_ERR("Can not create instance - "
				"already existing: %u", obj_inst_id);
			return NULL;
		}
	}

	for (index = 0; index < MAX_INSTANCE_COUNT; index++) {
		if (!inst[index].obj) {
			break;
		}
	}

	if (index >= MAX_INSTANCE_COUNT) {
		LOG_ERR("Can not create instance - "
			"no more room: %u", obj_inst_id);
		return NULL;
	}

	/* default values */
	adc_data[index][0] = '\0';
	adc_data_priority[index] = 0U;
	adc_data_creation_time[index] = 0U;
	adc_data_description[index][0] = '\0';
	adc_data_description[index][0] = '\0';
	adc_app_id[index] = 0U;

	(void)memset(res[index], 0,
		     sizeof(res[index][0]) * ARRAY_SIZE(res[index]));
	init_res_instance(res_inst[index], ARRAY_SIZE(res_inst[index]));

	/* initialize instance resource data */
	INIT_OBJ_RES(APP_DATA_CONTAINER_DATA, res[index], i,
			  res_inst[index], j, 1, true,
			  adc_data[index], MAX_DATA_LEN,
			  NULL, NULL, NULL, NULL);
	INIT_OBJ_RES_DATA(APP_DATA_CONTAINER_DATA_PRIOTRITY, res[index], i,
			  res_inst[index], j,
			  &adc_data_priority[index], sizeof(*adc_data_priority));
	INIT_OBJ_RES_DATA(APP_DATA_CONTAINER_DATA_CREATION_TIME, res[index], i,
			  res_inst[index], j,
			  &adc_data_creation_time[index], sizeof(*adc_data_creation_time));
	INIT_OBJ_RES_DATA(APP_DATA_CONTAINER_DATA_DESCRIPTION, res[index], i,
			  res_inst[index], j,
			  adc_data_description[index], 32);
	INIT_OBJ_RES_DATA(APP_DATA_CONTAINER_DATA_FORMAT, res[index], i,
			  res_inst[index], j,
			  adc_data_format[index], 32);
	INIT_OBJ_RES_DATA(APP_DATA_CONTAINER_APP_ID, res[index], i,
			  res_inst[index], j,
			  &adc_app_id[index], sizeof(*adc_app_id));

	inst[index].resources = res[index];
	inst[index].resource_count = i;
	LOG_DBG("Create LWM2M ADC instance: %d", obj_inst_id);

	return &inst[index];
}

int lwm2m_adc_inst_id_to_index(u16_t obj_inst_id)
{
	int i;

	for (i = 0; i < MAX_INSTANCE_COUNT; i++) {
		if (inst[i].obj && inst[i].obj_inst_id == obj_inst_id) {
			return i;
		}
	}

	return -ENOENT;
}

int lwm2m_adc_index_to_inst_id(int index)
{
	if (index >= MAX_INSTANCE_COUNT) {
		return -EINVAL;
	}

	/* not instanstiated */
	if (!inst[index].obj) {
		return -ENOENT;
	}

	return inst[index].obj_inst_id;
}

static int lwm2m_app_data_container_init(struct device *dev)
{
	app_data_container.obj_id = LWM2M_OBJECT_APP_DATA_CONTAINER_ID;
	app_data_container.fields = fields;
	app_data_container.field_count = ARRAY_SIZE(fields);
	app_data_container.max_instance_count = MAX_INSTANCE_COUNT;
	app_data_container.create_cb = adc_create;
	lwm2m_register_obj(&app_data_container);

	return 0;
}

SYS_INIT(lwm2m_app_data_container_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
