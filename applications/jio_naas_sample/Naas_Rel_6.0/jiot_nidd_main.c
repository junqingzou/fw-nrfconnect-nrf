/* ===============================START OF FILE====================================================
*
* Jio Platforms Limited.
* CONFIDENTIAL
* __________________
*
*  Copyright (C) 2021 Jio Platforms Limited -
*
*  ALL RIGHTS RESERVED.
*
* NOTICE:  All information including computer software along with source code and associated
* documentation contained herein is, and remains the property of Jio Platforms Limited. The
* intellectual and technical concepts contained herein are proprietary to Jio Platforms Limited.
* and are protected by copyright law or as trade secret under confidentiality obligations.
* Dissemination, storage, transmission or reproduction of this information in any part or full
* is strictly forbidden unless prior written permission along with agreement for any usage right
* is obtained from Jio Platforms Limited.
*
* This file contains reference code and to be used as reference only. Please note, this code 
* is not expected to be compiled directly.
*
* =============================================================================================== */

/* ===============================FILE REVISION HISTORY============================================
  Date         Author(RIL EMail ID)      CLIM ID(if applicable)       Detail of changes
--------       --------------------      ----------------------       -----------------
15-06-2021            JPL                         NONE                        CREATED
===============================ENDOF REVISION HISTORY============================================ */

/* ===============================ABOUT THIS FILE================================================ */

/**
* @file jiot_nidd_main.c
* @brief This file has implementation for jiot_nidd_api.h. This file implements Jio's server-interface over NIDD bearer.
*/
/*=================================END ABOUT THIS FILE=========================================== */

/* ===============================INCLUDE START================================================= */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "jiot_nidd_api.h"
#include "jiot_nidd_utils.h"
#include "jiot_nidd_plat_abs.h"
#if defined(CONFIG_NAAS_NORDIC)
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(naas, CONFIG_NAAS_LOG_LEVEL);
#endif

/* ===============================INCLUDE END=================================================== */

/* ===============================DEFINE START================================================= */
#define JIOT_NIDD_TPC_PLD_MSGID_LNGTH_FLD		  2
#define JIOT_NIDD_TPC_PAYLOAD_SIZE                2
#define JIOT_NIDD_TRANS_ID_LEN					  12
#define JIOT_NIDD_APP_ID_LEN					  10
#define API_VERSION                               1
#define EVENTS                                    "alerts"
#define JIOT_NIDD_DEVICE_MODEL                    20
#define JIOT_NIDD_C2D_CHANNEL                     40
#define JIOT_NIDD_PROTOCOL                        20
#define JIOT_NIDD_METADATA_BODY_FMT 	"{\r\n\"make\":\"%s\",\r\n\"domain\":\"%s\",\r\n\"model\":\"%s\",\r\n\
\"manufacturer\":\"%s\",\r\n\"dmtype\":\"%s\",\r\n\"devicetype\":\"%s\",\r\n\"protocol\":%s,\r\n\"c2dchannel\":%s,\r\n\
\"apps\":%s,\r\n\"firmware\":\"%s\",\r\n\"os\":\"%s\"\r\n}"

#define JIOT_NIDD_ACT_DEV_REQ_FMT	            "{\"api-version\":\"%d\",\"appName\":\"JioThingsApp\",\"uid\":\"%s\",\"meta_data\":%s}"
#define JIOT_NIDD_META_DATA_REQ_FMT             "{\"api-version\":\"%d\",\"uid\":\"%s\",\"meta_data\":%s}"
#define JIOT_NIDD_ACT_APP_REQ_FMT	            "{\"api-version\":\"%d\",\"appName\":\"%s\",\"uid\":\"%s\"}"
#define JIOT_NIDD_TOPIC_FMT                     "jioiot/%s/%s/%s/uc/fwd/%s"
#define JIO_NWTK_APN                            "JioNPIoT"
#define DEVICE_TYPE                             "TelemetryDevice"
#define DFLT_MAKE				                "jio"
#define DFLT_MANUFACTURER		                "Coretek"
#define DFLT_VENDOR				                "Mediatek"
#define DFLT_DEVTYPE			                "JNB3150"
#define DFLT_MODEL			 	                DFLT_DEVTYPE
#define DFLT_SWVER				                "[{\"name\":\"JNB3150\",\"version\":\"0.3.18\"}]"
#define DFLT_FWVER				                "JNB3150_0.1010"
#define DFLT_DOMAIN				                "jioiot"
#define DFLT_OS_VERSION	                        "FRTS_1.0"
#define JTOS_TELEMETRY_CONFIG_DMTYPE_RBOMA		"rb_oma"
#define DEVICEMODEL                             "jvt1440"
#define CONF_C2DCHANNEL                         "[\"mqtt\",\"nidd\"]"
#define CONF_PROTOCOL                           "[\"mqtt\",\"coap\"]"

#define CONF_NIDDFS_ROOT		                "niddfs"
#define CONF_FILENAME			                "cfg"

static char g_conf_make[6] = DFLT_MAKE;
static char g_conf_manufacturer[20] = DFLT_MANUFACTURER;
//static char g_conf_vendor[20] = DFLT_VENDOR;
static char g_conf_dmtype[20] = DFLT_DEVTYPE;
static char g_conf_rb_dmtype[20] = JTOS_TELEMETRY_CONFIG_DMTYPE_RBOMA;
static char g_device_model[JIOT_NIDD_DEVICE_MODEL] = DFLT_MODEL;
static char g_conf_swversion[256] = DFLT_SWVER;
static char g_conf_fwversion[40] = DFLT_FWVER;
static char g_conf_dm_domain[20] = DFLT_DOMAIN;
static char g_conf_os_version[20] = DFLT_OS_VERSION;
static char g_conf_c2d_channel[JIOT_NIDD_C2D_CHANNEL] = CONF_C2DCHANNEL;
static char g_conf_protocol[JIOT_NIDD_PROTOCOL] = CONF_PROTOCOL;
/* ===============================DEFINE END================================================= */

/* ===============================ENUM START================================================== */

/* ===============================ENUM END================================================== */

//* ===============================STRUCTURE START================================================== */
/**
 * @brief jiot_nidd_app_act_param store the app activation data receive from server during registration
 *
 */
typedef struct jiot_nidd_app_params {
    char *plmid;
    char *appName;
    char *svc;
    char *vid;
    char *aid;
    char *eid;
    jiot_nidd_callback *cb_options;
    jiot_nidd_meta_data_t *app_metaData;
}jiot_nidd_app_params_t;

/**
 * @brief jiot_nidd_app_act_param_holder store the list of app activation data receive from server during registration
 *
 */
typedef struct jiot_nidd_app_params_holder {
    jiot_nidd_app_params_t *app_params;
    struct jiot_nidd_app_params_holder *next;
}jiot_nidd_app_params_holder_t;

/**
 * @brief jiot_nidd_msgid_holder store msg details in the list.
 *
 */
typedef struct jiot_nidd_msgid_holder {
    uint16_t msgid;
    void *ptr;
    struct jiot_nidd_msgid_holder *next;
}jiot_nidd_msgid_holder_t;

/**
 * @brief jiot_nidd_send_params_t required to sending the data on NIDD
 */
typedef struct jiot_nidd_send_param {
    uint8_t *payload_msg;
    uint16_t payload_len;
	uint16_t msgid;
    void *appContext;
    jiot_nidd_delivery_mode_e Mode;
}jiot_nidd_send_param_t;

/**
 * @brief jiot_nidd_sendPayload_t required to store the sendPayload data of NIDD.
 */
typedef struct jiot_nidd_sendPayload {
    unsigned char *data;
    unsigned int size;
}jiot_nidd_sendPayload_t;

/* ===============================STRUCTURE END================================================== */

/* ===============================GLOBAL VARIABLE START=========================================== */
  static jiot_nidd_osal_message_processor_t *nidd_msg_processor_handler = NULL;
  static jiot_nidd_osal_semaphore_t *nidd_sem_handler = NULL;
  static jiot_nidd_msgid_holder_t *nidd_msgid_hnode = NULL;
  static jiot_nidd_app_params_holder_t *app_params_hnode = NULL;
  static uint32_t jiot_nidd_id;
  static bool is_dev_act = false;
  static bool is_init_done = false;
/* ===============================GLOBAL VARIABLE END=========================================== */

/* ===============================FORWARD DECLARATION START=========================================== */
void jiot_nidd_send_c2d_ack(char *trans_id, jiot_nidd_cmn_header_t *header);
/* ===============================FORWARD DECLARATION END=========================================== */

/* ===============================FUNCTION START================================================== */
/*-----------------------------------------------------------------------------------------------*/
/**
 * @brief This function gets the path where the config file should be stored
 * @ param[in] root - Root path if any
 * @ param[in] filename - name of the file
 *
 * @return Absolute path along with the filename is returned if successfull else NULL
 *
 */
char* jiot_nidd_conf_gen_getfilepath(char *root,char *filename)
{
    int length = 0;
    char *file_path = NULL;
    if(root != NULL)
    length = snprintf(NULL, 0, "/%s/%s",root, filename)+1;
    else
    length = strlen(filename)+1;
    file_path = (char *)jiot_nidd_utility_calloc(length, sizeof(char));
    if (!file_path)
    {
        JIOT_NIDD_LOG_E("Calloc Failed for config file path");
        return NULL;
    }
    if(root != NULL)
        snprintf(file_path, length, "/%s/%s", root, filename);
    else
        strncpy(file_path, filename, strlen(filename));
    JIOT_NIDD_LOG_D("filepath = %s", file_path);
    return file_path;
}
/*-----------------------------------------------------------------------------------------------*/
/** @brief This function creates the config file if not present
 *	
 *  @return  true if success . false if creation failed
 */
bool jiot_nidd_conf_gen_create_file()
{
    char *filename = NULL;
    bool ret_val = false;
    void *fd = NULL;
    char *buf = NULL;
//    int size = 0;
    int length = 0;
    int pal_ret = -1;
    filename = jiot_nidd_conf_gen_getfilepath(CONF_NIDDFS_ROOT,CONF_FILENAME);

    if (filename)
    {
        // Deleting the existing file
        if (jiot_nidd_utility_file_open((unsigned char *)filename, E_NIDD_PAL_FILE_FLAG_READ, &fd) == 0)
        {
#if defined(CONFIG_NAAS_NORDIC)
            jiot_nidd_utility_file_close(fd);
#else
            jiot_client_PAL_File_close(fd);
#endif
            if (jiot_nidd_utility_file_remove(filename) != 0)
            {
                JIOT_NIDD_LOG_E("Config File Deletion failed");
                ret_val = false;
                goto memfree;
            }
        }
        if (jiot_nidd_utility_file_open(filename, E_NIDD_PAL_FILE_FLAG_WRITE, &fd) == 0)
        {
            length = snprintf(NULL,0,JIOT_NIDD_METADATA_BODY_FMT,g_conf_make,g_conf_dm_domain,g_device_model,\
            g_conf_manufacturer,g_conf_rb_dmtype,g_conf_dmtype,g_conf_protocol,g_conf_c2d_channel,\
            g_conf_swversion,g_conf_fwversion,g_conf_os_version)+1;

            buf = (char *)jiot_nidd_utility_calloc(length,sizeof(char));
            if(!buf)
            {
                JIOT_NIDD_LOG_E("Memory allocation for buf : Failed");
                return NULL;
            }
            else
            {
                sprintf(buf,JIOT_NIDD_METADATA_BODY_FMT,g_conf_make,g_conf_dm_domain,g_device_model,\
                g_conf_manufacturer,g_conf_rb_dmtype,g_conf_dmtype,g_conf_protocol,g_conf_c2d_channel,\
                g_conf_swversion,g_conf_fwversion,g_conf_os_version);
                pal_ret = jiot_nidd_utility_file_write(fd, 0, length, (unsigned char *)buf);
                if (pal_ret >= 0)
                {
                    ret_val = true;
                }
                else
                {
                    JIOT_NIDD_LOG_E("Config File Write Failed  %d", pal_ret);
                    jiot_nidd_utility_file_remove(filename);
                    ret_val = false;
                }
                jiot_nidd_utility_file_close(fd);
                goto memfree;
            }
        }
        else{
            JIOT_NIDD_LOG_E("Failed to Create cfg file");
            ret_val = false;
            goto memfree;
        }
    }
    else{
        return false;
    }

    memfree:
    if(filename)
        jiot_nidd_utility_free((void *)filename);

    return ret_val;
}
/*-----------------------------------------------------------------------------------------------*/
/**
 * @brief This function set the device model to config file
 * @ param[in] DeviceModel - Params needs to be set in config file
 *
 * @return None
 */
void jiot_nidd_conf_gen_set_device_model(char* DeviceModel)
{
    if(NULL == DeviceModel)
    {
        JIOT_NIDD_LOG_E("Param DeviceModel is NULL");
        return;
    }
    memset(g_device_model,'\0', JIOT_NIDD_DEVICE_MODEL*sizeof(char));
    strncpy(g_device_model,DeviceModel, strlen(DeviceModel));
    JIOT_NIDD_LOG_I("Conf_device_model %s ", g_device_model);
    if(jiot_nidd_conf_gen_create_file() == false)
    JIOT_NIDD_LOG_E("Config File Update Failed for Device Model");
}
/*-----------------------------------------------------------------------------------------------*/
/**
 * @brief This function set the c2d channel to config file
 * @ param[in] c2dChannel - Params needs to be set in config file
 *
 * @return None
 */
void jiot_nidd_conf_gen_set_c2d_channel(char* c2dChannel)
{
    char *conf_c2d_channel = "[\"nidd\"]";
    if(NULL == c2dChannel)
    {
        JIOT_NIDD_LOG_E("Param c2dChannel is NULL");
        return;
    }
    if(!strcmp(c2dChannel,"nidd"))
    {
        memset(g_conf_c2d_channel,'\0', JIOT_NIDD_C2D_CHANNEL*sizeof(char));
        strncpy(g_conf_c2d_channel,conf_c2d_channel, strlen(conf_c2d_channel));
        JIOT_NIDD_LOG_I("Conf_c2d_channel %s ", g_conf_c2d_channel);
        if(jiot_nidd_conf_gen_create_file() == false)
        JIOT_NIDD_LOG_E("Config File Update Failed for Device Model");
    }
    else{
        JIOT_NIDD_LOG_E("Invalid Cfg Params");
        return;
    }
}
/*-----------------------------------------------------------------------------------------------*/
/**
 * @brief This function set the protocol to config file
 * @ param[in] Protocol - Params needs to be set in config file
 * @return None
 */
void jiot_nidd_conf_gen_set_protocol(char* Protocol)
{
    char *conf_protocol_mqtt = "[\"mqtt\"]";
    char *conf_protocol_coap = "[\"coap\"]";
    if(NULL == Protocol)
    {
       JIOT_NIDD_LOG_E("Param Protocol is NULL");
       return;
    }

    if(!strcmp(Protocol, "mqtt"))
    {
        memset(g_conf_protocol, '\0', JIOT_NIDD_PROTOCOL*sizeof(char));
        strncpy(g_conf_protocol,conf_protocol_mqtt, strlen(conf_protocol_mqtt));
        JIOT_NIDD_LOG_I("Conf_protocol %s ", g_conf_protocol);

        if(jiot_nidd_conf_gen_create_file() == false)
        JIOT_NIDD_LOG_E("Config File Update Failed for Protocol");
    }
    else if(!strcmp(Protocol,"coap"))
    {
        memset(g_conf_protocol, '\0', JIOT_NIDD_PROTOCOL*sizeof(char));
        strncpy(g_conf_protocol,conf_protocol_coap, strlen(conf_protocol_coap));
        JIOT_NIDD_LOG_I("Conf_protocol %s ", g_conf_protocol);

        if(jiot_nidd_conf_gen_create_file() == false)
        JIOT_NIDD_LOG_E("Config File Update Failed for Protocol");
    }
    else
    {
        JIOT_NIDD_LOG_E("Invalid Cfg Params");
        return;
    }
}
/*-----------------------------------------------------------------------------------------------*/
/** @brief This function reads the details from config file
 *
 *   @return if success E_NIDD_CONFIG_SUCCESS, E_NIDD_CONFIG_FAILURE if creation failed
 */
int jiot_nidd_readconfigfile(void)
{
    void *fptr = NULL;
    char * buff = NULL;
    int size = 0;
    int retval = 0;
    char *filename = NULL;
    jiot_nidd_json_err_e parse_result = {0};
    jiot_nidd_json_info_t info = {0};
    char *device_model = NULL;
    char *conf_protocol = NULL;
    char *conf_c2d_channel = NULL;

    filename = jiot_nidd_conf_gen_getfilepath(CONF_NIDDFS_ROOT,CONF_FILENAME);
    if (filename)
    {
        int file = jiot_nidd_utility_file_open(filename, E_NIDD_PAL_FILE_FLAG_READ,&fptr);
        if (fptr == NULL)
        {
            JIOT_NIDD_LOG_E("Config File Not Found ");
            return E_NIDD_CONFIG_FAILURE;
        }

        retval = jiot_nidd_utility_file_seek(fptr,0L,E_NIDD_PAL_FILE_WHENCE_END);
        if(retval < 0 )
        {
            JIOT_NIDD_LOG_E("File seek failed");
            jiot_nidd_utility_file_close(fptr);
            return E_NIDD_CONFIG_FAILURE;
        }
        size = jiot_nidd_utility_file_tell(fptr);

        if(size < 0)
        {
            JIOT_NIDD_LOG_E("File tell failed");
            jiot_nidd_utility_file_close(fptr);
            return E_NIDD_CONFIG_FAILURE;
        }
        jiot_nidd_utility_file_seek(fptr,0L,E_NIDD_PAL_FILE_WHENCE_START);

        buff = (char *)jiot_nidd_utility_calloc(size+1,sizeof(char));
        if(!buff)
        {
            JIOT_NIDD_LOG_E("Calloc failed for config buff");
            jiot_nidd_utility_file_close(fptr);
            return E_NIDD_CONFIG_FAILURE;
        }
        file = jiot_nidd_utility_file_read(fptr,0,size,(unsigned char *)buff);
        jiot_nidd_utility_file_close(fptr);

        parse_result = jiot_nidd_json_parse_get_value(buff, size,"model",&device_model,&info);
        if(parse_result != E_NIDD_JSON_SUCCESS)
        {
            JIOT_NIDD_LOG_E("parse failed with Error Code : %d",parse_result);
            goto memfree;
        }

        strncpy(g_device_model,device_model,strlen(device_model));
        parse_result = jiot_nidd_json_parse_get_value(buff, size,"protocol",&conf_protocol,&info);
        if(parse_result != E_NIDD_JSON_SUCCESS)
        {
            JIOT_NIDD_LOG_E("parse failed with Error Code : %d",parse_result);
            goto memfree;
        }
        strncpy(g_conf_protocol,conf_protocol,strlen(conf_protocol));
        parse_result = jiot_nidd_json_parse_get_value(buff, size,"c2dchannel",&conf_c2d_channel,&info);
        if(parse_result != E_NIDD_JSON_SUCCESS)
        {
            JIOT_NIDD_LOG_E("parse failed with Error Code : %d",parse_result);
            goto memfree;
        }
        strncpy(g_conf_c2d_channel,conf_c2d_channel,strlen(conf_c2d_channel));
    }
    else{
        JIOT_NIDD_LOG_E("Config Gen File Not Found");
    }

    memfree:
        if(device_model)
            jiot_nidd_utility_free(device_model);
        if(conf_protocol)
            jiot_nidd_utility_free(conf_protocol);
        if(conf_c2d_channel)
            jiot_nidd_utility_free(conf_c2d_channel);
        if(buff)
            jiot_nidd_utility_free(buff);
        if(filename)
            jiot_nidd_utility_free(filename);

    return E_NIDD_CONFIG_SUCCESS;
}

/*-----------------------------------------------------------------------------------------------*/
/**
* @brief  This function removes the misId from the linked list used to identify the c2d/d2c message.
*   @param[in] msg_id - message Identity.  
*   @return  jiot_nidd_error_code_e
*
*/
jiot_nidd_error_code_e jiot_nidd_remove_msgid_from_list(uint16_t msg_id)
{
    JIOT_NIDD_LOG_D("entry : %s", __func__);
    jiot_nidd_msgid_holder_t *curr_mem = nidd_msgid_hnode;
    jiot_nidd_msgid_holder_t *prev_mem = NULL;

    if(curr_mem == NULL)
    {
        JIOT_NIDD_LOG_E("curr mem is NULL");
        return E_NIDD_ERROR_INVALID_PARAM;
    }
    else
    {
        do
        {
            if(curr_mem->msgid == msg_id)
            {
				if(prev_mem == NULL)
					nidd_msgid_hnode = curr_mem->next;
				else
					prev_mem->next = curr_mem->next;

				jiot_nidd_utility_free((void *)curr_mem);
				return E_NIDD_SUCCESS;
			}
			prev_mem = curr_mem;
			curr_mem = curr_mem->next;
		}while(curr_mem != NULL);

		return E_NIDD_ERROR_FAILURE;
	}	
}
/*-----------------------------------------------------------------------------------------------*/
/**
* @brief This function finds the application context corresponding to the message ID and returns the same
*   @param[in] msgId - message ID 
*   @return  application context if registered else NULL
*
*/
void *jiot_nidd_find_msgid_in_list(uint16_t msg_id)
{
    JIOT_NIDD_LOG_D("entry: %s", __func__);
	jiot_nidd_msgid_holder_t *find_list = nidd_msgid_hnode;
    if(find_list == NULL)
    {
        JIOT_NIDD_LOG_E("find list is NULL");
        return NULL;
    }
	while(find_list != NULL)
	{
		if(find_list->msgid == msg_id)
			return find_list->ptr;
		find_list= find_list->next;
	}
    JIOT_NIDD_LOG_E("No msgID is found");
	return NULL;
}

/*-----------------------------------------------------------------------------------------------*/
/**
* @brief  This function adds the application context with message ID to the linked list used to identify the context during D2C ack
*   @param[in] ptr - application Context.
*   @param[in] msg_id - message Identity.
*   @return  it will return jiot_nidd_error_code_e
*
*/
jiot_nidd_error_code_e jiot_nidd_add_msgid_to_list(uint16_t msg_id, void *ptr)
{
    JIOT_NIDD_LOG_D("entry : %s",__func__);
    jiot_nidd_msgid_holder_t *new_mem = NULL;
    jiot_nidd_msgid_holder_t *temp_list = NULL;

    new_mem = (jiot_nidd_msgid_holder_t *)jiot_nidd_utility_malloc(sizeof(jiot_nidd_msgid_holder_t));
    if(new_mem == NULL)
    {
        JIOT_NIDD_LOG_E("malloc failed");
        return E_NIDD_ERROR_NO_MEMORY;
    }
    memset(new_mem, 0x00, sizeof(jiot_nidd_msgid_holder_t));
    new_mem->msgid = msg_id;
    new_mem->ptr = ptr;
    new_mem->next = NULL;

    if(nidd_msgid_hnode == NULL)
    {
        nidd_msgid_hnode = new_mem;
    }	
	else
	{
		temp_list = nidd_msgid_hnode;
		while(temp_list->next != NULL)
			temp_list = temp_list->next;
		temp_list->next = new_mem;
	}
    return E_NIDD_SUCCESS;
}

/*-----------------------------------------------------------------------------------------------*/
/**
* @brief  This function removes the app activation params receive during app Activation from the list 
*   @param[in] appId - application Identity  
*   @return  jiot_nidd_error_code_e
*
*/
jiot_nidd_error_code_e jiot_nidd_remove_app_params_from_list(jiot_nidd_app_params_t *app_parameters)
{
    JIOT_NIDD_LOG_D("entry : %s",__func__);
    jiot_nidd_app_params_holder_t *curr_mem = app_params_hnode;
    jiot_nidd_app_params_holder_t *prev_mem = NULL;
    if(curr_mem == NULL || app_parameters == NULL)
    {
        JIOT_NIDD_LOG_E("curr_mem or app_parameters is NULL");
        return E_NIDD_ERROR_INVALID_PARAM;
    }	
	else
	{
		do
		{
			
            if((curr_mem->app_params) == (app_parameters))
			{
                if(curr_mem->app_params->plmid)
                    jiot_nidd_utility_free(curr_mem->app_params->plmid);
                if(curr_mem->app_params->svc)
                    jiot_nidd_utility_free(curr_mem->app_params->svc);
                if(curr_mem->app_params->eid)
                    jiot_nidd_utility_free(curr_mem->app_params->eid);
                if(curr_mem->app_params->vid)
                    jiot_nidd_utility_free(curr_mem->app_params->vid);
                if(curr_mem->app_params->aid)
                    jiot_nidd_utility_free(curr_mem->app_params->aid);
                if(curr_mem->app_params->appName)
                    jiot_nidd_utility_free(curr_mem->app_params->appName);
                if(curr_mem->app_params->app_metaData)
                    jiot_nidd_utility_free(curr_mem->app_params->app_metaData);
                if(curr_mem->app_params)
                    jiot_nidd_utility_free(curr_mem->app_params);
                
				if(prev_mem == NULL)
					app_params_hnode = curr_mem->next;
				else
					prev_mem->next = curr_mem->next;

				jiot_nidd_utility_free((void *)curr_mem);
				return E_NIDD_SUCCESS;
			}
			prev_mem = curr_mem;
			curr_mem= curr_mem->next;
		}while(curr_mem != NULL);

		return E_NIDD_ERROR_FAILURE;
	}
}
/*-----------------------------------------------------------------------------------------------*/
/**
* @brief This function finds application parameters corresponding to the appName and returns the adress of jiot_nidd_app_params_t
*   @param[in] appID - Application ID
*   @return  jiot_nidd_app_params_t* if application is registered else NULL
*
*/
jiot_nidd_app_params_t *jiot_nidd_find_app_params_in_list(char *appId)
{
    JIOT_NIDD_LOG_D("entry : %s",__func__);
    jiot_nidd_app_params_holder_t *find_list = app_params_hnode;
    if(find_list == NULL || appId == NULL)
    {
        JIOT_NIDD_LOG_E("App_params_hnode or appName is NULL");
        return NULL;
    }
    while(find_list !=NULL)
    {
        if(!strcmp(find_list->app_params->aid,appId))
            return find_list->app_params;

        find_list = find_list->next;
    }
    JIOT_NIDD_LOG_W("No appId present related to this appName");
    return NULL;

}

/*-----------------------------------------------------------------------------------------------*/
/**
* @brief  This function adds the application activation parameter to the list used to identify the application
*   @param[in] app_params_t - address of struct jiot_nidd_app_params_t 
*   @return  it will return jiot_nidd_error_code_e
*
*/
jiot_nidd_error_code_e jiot_nidd_add_app_params_to_list(jiot_nidd_app_params_t *app_params_t)
{

    JIOT_NIDD_LOG_D("entry : %s",__func__);
    jiot_nidd_app_params_holder_t *new_mem = NULL;
    jiot_nidd_app_params_holder_t *temp_list = NULL;

    new_mem = (jiot_nidd_app_params_holder_t *)jiot_nidd_utility_malloc(sizeof(jiot_nidd_app_params_holder_t));
    if(new_mem == NULL)
    {
        JIOT_NIDD_LOG_E("Memory Allocation for new_mem is Failed");
        return E_NIDD_ERROR_NO_MEMORY;
    }
    memset(new_mem, 0x00, sizeof(jiot_nidd_app_params_holder_t));
    new_mem->app_params = app_params_t;
    new_mem->next = NULL;

    if(app_params_hnode == NULL)
    {
        app_params_hnode = new_mem;
    }
    else
    {
        temp_list = app_params_hnode;
		while(temp_list->next != NULL)
			temp_list = temp_list->next;
		temp_list->next = new_mem;
    }

    return E_NIDD_SUCCESS;
}

/*-----------------------------------------------------------------------------------------------*/
/**
* @brief  This function is the entry function of NIDD message processor
*   @return void
*
*/
void jiot_nidd_worker_start(void)
{
    JIOT_NIDD_LOG_D("entry: %s", __func__);
    do
    {
        jiot_nidd_osal_message_t *msg = NULL;
		JIOT_NIDD_LOG_I("Message Processor : Waiting for message on queue");
        msg = jiot_nidd_osal_message_processor_readQueue(nidd_msg_processor_handler);
		if (msg != NULL) 
		{	
			JIOT_NIDD_LOG_I("Message Processor : processing a message");
			if(msg)
			{
				if (msg->msgHandler)
				{
					msg->msgHandler(msg->message);
				}
				jiot_nidd_utility_free((void *)msg);
			}
        }
    } while (!jiot_nidd_osal_message_processor_isdone(nidd_msg_processor_handler));
	jiot_nidd_osal_message_processor_terminate(nidd_msg_processor_handler);
	nidd_msg_processor_handler = NULL;
    return ;
}

/*-----------------------------------------------------------------------------------------------*/
/**
* @brief  This function parses the information recieves as part of device/ app activation
*   @param[in] json_string - data received from the server  
*   @param[in] payload_len - length of the data received from the server
*	@return jiot_nidd_error_code_e
*/
jiot_nidd_error_code_e jiot_nidd_parse_appact_response(jiot_nidd_app_params_t *app_act_param, char *json_string, uint16_t payload_len)
{
    JIOT_NIDD_LOG_D("entry : %s",__func__);
    jiot_nidd_json_err_e parse_result = {0};
    //int size = payload_len;
    //int length = 0;
    int retval = E_NIDD_ERROR_FAILURE;
    jiot_nidd_json_info_t info = {0};
    
    if(app_act_param == NULL)
    {
        JIOT_NIDD_LOG_E("app_act_param is NULL");
        return E_NIDD_ERROR_INVALID_PARAM;
    }
        

    if(!is_dev_act)
    {
        parse_result = jiot_nidd_json_parse_get_value(json_string, payload_len,"appName",&app_act_param->appName, &info);
        if(parse_result != E_NIDD_JSON_SUCCESS)
        {
            JIOT_NIDD_LOG_E("parse failed with Error Code : %d",parse_result);
            retval = E_NIDD_ERROR_FAILURE;
            goto memfree;
        }

        parse_result = jiot_nidd_json_parse_get_value(json_string, payload_len, "svc", &app_act_param->svc, &info);
        if(parse_result != E_NIDD_JSON_SUCCESS)
        {
            JIOT_NIDD_LOG_E("parse failed with Error Code : %d",parse_result);
            retval = E_NIDD_ERROR_FAILURE;
            goto memfree;
        }

        parse_result = jiot_nidd_json_parse_get_value(json_string, payload_len, "eid", &app_act_param->eid, &info);
        if(parse_result != E_NIDD_JSON_SUCCESS)
        {
            JIOT_NIDD_LOG_E("parse failed with Error Code : %d",parse_result);
            retval = E_NIDD_ERROR_FAILURE;
            goto memfree;
        }

        parse_result = jiot_nidd_json_parse_get_value(json_string, payload_len, "vid", &app_act_param->vid, &info);
        if(parse_result != E_NIDD_JSON_SUCCESS)
        {
            JIOT_NIDD_LOG_E("parse failed with Error Code : %d",parse_result);
            retval = E_NIDD_ERROR_FAILURE;
            goto memfree;
        }

        parse_result = jiot_nidd_json_parse_get_value(json_string, payload_len, "aid", &app_act_param->aid, &info);
        if(parse_result != E_NIDD_JSON_SUCCESS)
        {
            JIOT_NIDD_LOG_E("parse failed with Error Code : %d",parse_result);
            retval = E_NIDD_ERROR_FAILURE;
            goto memfree;
        }

        retval =  E_NIDD_SUCCESS;
    }
    else if(is_dev_act)
    {
        parse_result = jiot_nidd_json_parse_get_value(json_string, payload_len, "pld", &app_act_param->plmid, &info);
        if(parse_result != E_NIDD_JSON_SUCCESS)
        {
            JIOT_NIDD_LOG_E("parse failed with Error Code : %d",parse_result);
            retval = E_NIDD_ERROR_FAILURE;
            goto memfree;
        }
        else
        {
            retval = E_NIDD_SUCCESS;
        }
    }

    return retval;
        
    memfree:
        if(app_act_param->appName)
            jiot_nidd_utility_free(app_act_param->appName);
        if(app_act_param->svc)
            jiot_nidd_utility_free(app_act_param->svc);
        if(app_act_param->eid)
            jiot_nidd_utility_free(app_act_param->eid);
        if(app_act_param->vid)
            jiot_nidd_utility_free(app_act_param->vid);
        if(app_act_param->aid)
            jiot_nidd_utility_free(app_act_param->aid);
        if(app_act_param->plmid)
            jiot_nidd_utility_free(app_act_param->plmid);

    return retval;
}

/*-----------------------------------------------------------------------------------------------*/
/**
* @brief  This function handles the activation of app message from the server
*   @param[in] msgid - msg id receive in the activation request
*   @param[in] payload - payload received from the server
*   @param[in] length - length of the payload
*   @return jiot_nidd_error_code_e 
*
*/
jiot_nidd_error_code_e jiot_nidd_received_appact_response(uint16_t msgid , char *payload, uint16_t length)
{
    JIOT_NIDD_LOG_D("entry : %s",__func__);
    void *app_params = NULL;
    int retVal = E_NIDD_ERROR_FAILURE;

    app_params = jiot_nidd_find_msgid_in_list(msgid);
    if(app_params == NULL)
    {
        JIOT_NIDD_LOG_E("app_params is NULL");
        return E_NIDD_ERROR_INVALID_PARAM;
    }

    if(jiot_nidd_parse_appact_response((jiot_nidd_app_params_t *)app_params, payload, length) == E_NIDD_SUCCESS)
    {
        retVal = jiot_nidd_osal_semaphore_post(nidd_sem_handler);
        if(retVal != E_NIDD_OSAL_SUCCESS)
        {
            JIOT_NIDD_LOG_E("semaphore_post : Failure");
            jiot_nidd_remove_msgid_from_list(msgid);
            return E_NIDD_ERROR_FAILURE;
        }
        retVal = jiot_nidd_remove_msgid_from_list(msgid);
        if(retVal != E_NIDD_SUCCESS)
        {
            JIOT_NIDD_LOG_E("remove msgId from list : Failure");
            return E_NIDD_ERROR_FAILURE;
        }
    }
    else
    {
        JIOT_NIDD_LOG_E("Appact resp parse failed");
        retVal = jiot_nidd_remove_msgid_from_list(msgid);
        if(retVal != E_NIDD_SUCCESS)
        {
            JIOT_NIDD_LOG_E("remove msgId from list : Failure");
            return E_NIDD_ERROR_FAILURE;
        }
    }
        
    return E_NIDD_SUCCESS;
}

/*-----------------------------------------------------------------------------------------------*/
/**
* @brief  This function handles the deactivation of app message from the server //TODO
*   @param[in] msgid - msg id sent in the deactivation request
*   @return void
*
*/
void jiot_nidd_received_appdeact_response(uint16_t msgid)
{
    JIOT_NIDD_LOG_D("entry : %s",__func__);

}
/*-----------------------------------------------------------------------------------------------*/
/**
* @brief  This function notifies the message delivery to the application and also removes the msgid from the list
*   @param[in] msgid - msg id sent in the publish request
*   @return void
*
*/
void jiot_nidd_message_delivered_ack(uint16_t msgid)
{
    JIOT_NIDD_LOG_D("entry : %s",__func__);
    jiot_nidd_callback *appCallback = NULL;
    char msg_id_buf[10];
    appCallback = (jiot_nidd_callback *)jiot_nidd_find_msgid_in_list(msgid);

    if(appCallback != NULL)
    {
        snprintf(msg_id_buf,10,"%d",msgid);
        appCallback(E_NIDD_SEND_SUCCESS,(void *)&msg_id_buf,strlen(msg_id_buf));
        
        if(jiot_nidd_remove_msgid_from_list(msgid) != E_NIDD_SUCCESS)
        {
            JIOT_NIDD_LOG_E("nidd_remove MSGID  : Failed");
        }
    }
    else
    {
        JIOT_NIDD_LOG_W("Invalid msgId received");
    }
}
/*-----------------------------------------------------------------------------------------------*/
/**
* @brief  This function handles the meta data update response message from the server
*   @param[in] msgid - msg id receive in the activation request
*   @param[in] payload - payload received from the server
*   @param[in] length - length of the payload
*   @return jiot_nidd_error_code_e 
*
*/
jiot_nidd_error_code_e jiot_nidd_meta_data_message_response(uint16_t msgid , char *payload, uint16_t length)
{
    JIOT_NIDD_LOG_D("entry : %s",__func__);
    int retval = E_NIDD_ERROR_FAILURE;
    jiot_nidd_app_params_t *app_params = NULL;
    char *res_status = NULL;
    char msg_id_buf[10];
    jiot_nidd_json_err_e parse_result = {0};
    jiot_nidd_json_info_t info = {0};
    app_params = (jiot_nidd_app_params_t *)jiot_nidd_find_msgid_in_list(msgid);
    

    parse_result = jiot_nidd_json_parse_get_value(payload, length,"status",&res_status, &info);
    if(parse_result != E_NIDD_JSON_SUCCESS)
    {
        JIOT_NIDD_LOG_E("parse failed with Error Code : %d",parse_result);
        retval = E_NIDD_ERROR_FAILURE;
        goto memfree;
    }
    
    if(app_params != NULL)
    {
        snprintf(msg_id_buf,10,"%d",msgid);

        if(!strcmp(res_status, "200"))
        app_params->cb_options(E_NIDD_SEND_SUCCESS,&msg_id_buf,strlen(msg_id_buf));
        else
        app_params->cb_options(E_NIDD_SEND_FAILED,&msg_id_buf,strlen(msg_id_buf));

        retval = jiot_nidd_osal_semaphore_post(nidd_sem_handler);
        if(retval != E_NIDD_OSAL_SUCCESS)
        {
            JIOT_NIDD_LOG_E("semaphore_post : Failure");
            jiot_nidd_remove_msgid_from_list(msgid);
            return E_NIDD_ERROR_FAILURE;
        }
        if(jiot_nidd_remove_msgid_from_list(msgid) != E_NIDD_SUCCESS)
        {
            JIOT_NIDD_LOG_E("nidd_remove MSGID  : Failed");
        }
    }
    else
    {
        JIOT_NIDD_LOG_W("Invalid msgId received");
    }

    memfree:
        if(res_status)
            jiot_nidd_utility_free(res_status);

    return E_NIDD_SUCCESS;
}
/*-----------------------------------------------------------------------------------------------*/
/**
 * @brief This function call the application callbacks whenever C2D PUBLISH message is recived.
 *   @param[in] appId - application ID received in the c2d publish message.
 *   @param[in] payload - received payload
 *   @param[in] length -received payload length.
 *   @return void
 * 
 */
void jiot_nidd_onMessageArrived(char *appId, uint8_t *payload, uint16_t length)
{
    JIOT_NIDD_LOG_D("entry : %s",__func__);

    jiot_nidd_app_params_t *app_params = NULL;
    app_params = jiot_nidd_find_app_params_in_list(appId);
    if(app_params == NULL)
    {
        JIOT_NIDD_LOG_E("No application is registered for appId : %s Dropping the msg.... ", appId);
    }
    else
    {
        app_params->cb_options(E_NIDD_DATA, payload, length);
    }
}
/*-----------------------------------------------------------------------------------------------*/
/**
*  @brief This function receives the NIDD data coming from the server . 
*   @param[in] recv_DLM - contains data received from the server. 
*   @return void
*
*/
void jiot_nidd_receive_data(void *recv_DLM)
{
    JIOT_NIDD_LOG_D("entry : %s",__func__);
	jiot_nidd_data_t *data = (jiot_nidd_data_t *)recv_DLM;
    uint8_t *recv_buf = data->p_data;
	uint16_t parsed_len = 0;
	uint16_t msg_id = 0;
	uint16_t paylod_len = 0;
	jiot_nidd_cmn_header_t header = {0};
    static char prev_trans_id[JIOT_NIDD_TRANS_ID_LEN + 1] = {0};

    jiot_nidd_dumphex(data->p_data, data->data_len);

	parsed_len = sizeof(jiot_nidd_cmn_header_t);
	memcpy((void *)&header ,(void *)recv_buf,parsed_len);
	switch(header.msg_type)
	{
		case E_NIDD_MSG_PUBLISH:
			if(header.pkt_type == E_NIDD_PKT_PUBACK)
			{
				msg_id = (recv_buf[parsed_len] << 8) +  recv_buf[parsed_len+1];
				jiot_nidd_message_delivered_ack(msg_id);
				parsed_len += JIOT_NIDD_TPC_PLD_MSGID_LNGTH_FLD;
			}
			else
			{
				char temp[JIOT_NIDD_TRANS_ID_LEN +1] = {0};
				
				if(header.pkt_type == E_NIDD_PKT_PUBCON)
				{
					memcpy((void *)temp ,(void *)(recv_buf + parsed_len),JIOT_NIDD_TRANS_ID_LEN);
					parsed_len += JIOT_NIDD_TRANS_ID_LEN;
					jiot_nidd_send_c2d_ack(temp,&header);
                    if(strcmp(temp,prev_trans_id)==0)
                    {
                        JIOT_NIDD_LOG_W("Duplicate transaction ID.");
                        break;
                    }
                    strncpy(prev_trans_id,temp,JIOT_NIDD_TRANS_ID_LEN);
				}
				memcpy((void *)temp ,(void *)(recv_buf + parsed_len) ,JIOT_NIDD_APP_ID_LEN);
				temp[JIOT_NIDD_APP_ID_LEN] = 0x00;
                parsed_len += JIOT_NIDD_APP_ID_LEN;

				paylod_len = (recv_buf[parsed_len] << 8) + recv_buf[parsed_len+1];
				parsed_len += JIOT_NIDD_TPC_PLD_MSGID_LNGTH_FLD;
				if((data->data_len - parsed_len) == paylod_len)
				{
                    jiot_nidd_onMessageArrived(temp, recv_buf + parsed_len, paylod_len);
				}
				else
					JIOT_NIDD_LOG_W("Mismatch in length Actual payload len = %d, payload len in data= %d",\
					data->data_len - parsed_len,paylod_len);
			}
		    break;

		case E_NIDD_MSG_APPACTRESP:
			msg_id = (recv_buf[parsed_len] << 8) +  recv_buf[parsed_len+1];
			parsed_len += JIOT_NIDD_TPC_PLD_MSGID_LNGTH_FLD;

			paylod_len = (recv_buf[parsed_len] << 8) + recv_buf[parsed_len+1];
			parsed_len += JIOT_NIDD_TPC_PLD_MSGID_LNGTH_FLD;
			if((data->data_len - parsed_len) == paylod_len)
            {
                jiot_nidd_received_appact_response(msg_id,(char *)recv_buf + parsed_len,paylod_len);
            }
			else
            {
                JIOT_NIDD_LOG_W("Mismatch in length Actual payload len = %d, payload len in data= %d",\
					data->data_len - parsed_len,paylod_len);
            }
		    break;

		case E_NIDD_MSG_APPACT:
			 msg_id = (recv_buf[parsed_len] << 8) +  recv_buf[parsed_len+1];
			 parsed_len += JIOT_NIDD_TPC_PLD_MSGID_LNGTH_FLD;
			 JIOT_NIDD_LOG_I("Ack recvd App Act msg %d",msg_id);
		     break;

		case E_NIDD_MSG_APPDEACTRESP:
			 msg_id = (recv_buf[parsed_len] << 8) +  recv_buf[parsed_len+1];
			 parsed_len += JIOT_NIDD_TPC_PLD_MSGID_LNGTH_FLD;
			 jiot_nidd_received_appdeact_response(msg_id);
		     break;

        case E_NIDD_MSG_METADATA:
            if(header.pkt_type == E_NIDD_PKT_PUBACK)
			{
				msg_id = (recv_buf[parsed_len] << 8) +  recv_buf[parsed_len+1];
				JIOT_NIDD_LOG_I("Ack recvd for meta data update msg %d ",msg_id);
				parsed_len += JIOT_NIDD_TPC_PLD_MSGID_LNGTH_FLD;
			} 
            else if(header.pkt_type == E_NIDD_PKT_PUBCON)
			{
				msg_id = (recv_buf[parsed_len] << 8) +  recv_buf[parsed_len+1];
			    parsed_len += JIOT_NIDD_TRANS_ID_LEN;
			    paylod_len = (recv_buf[parsed_len] << 8) + recv_buf[parsed_len+1];
			    parsed_len += JIOT_NIDD_TPC_PLD_MSGID_LNGTH_FLD;
                paylod_len = (recv_buf[parsed_len] << 8) + recv_buf[parsed_len+1];
                parsed_len += JIOT_NIDD_TPC_PAYLOAD_SIZE;            

			    if((data->data_len - parsed_len) == paylod_len)
                {
                    jiot_nidd_meta_data_message_response(msg_id,(char *)recv_buf + parsed_len,paylod_len);
                }
			    else
                {
                JIOT_NIDD_LOG_W("Mismatch in length Actual payload len = %d, payload len in data= %d",\
					data->data_len - parsed_len,paylod_len);
                }
            }
            break;
		default:
			JIOT_NIDD_LOG_E("Msg Type not handled %d",header.msg_type);
		break;
	}
    if(recv_buf)
	    jiot_nidd_utility_free(recv_buf);
    if(recv_DLM)
	    jiot_nidd_utility_free(recv_DLM);
}

/*-----------------------------------------------------------------------------------------------*/
/** @brief Defining a callback function for the none-ip event 
*    
*   @param[in] event   Below are the explaination of each Events:
*                       1. case E_NIDD_PLAT_EVENT_CHANNEL_ACTIVATE_IND : return Active status of Non-Ip channel(means Non-IP connect and open a socket sucessfully)
*                       2. case E_NIDD_PLAT_EVENT_CHANNEL_DEACTIVATE_IND : return Deactiavte status of Non-Ip channel (means Non-IP disconnect and close the socket sucessfully)
*                       3. case E_NIDD_PLAT_EVENT_DATA_IND : return Downlink data and response.
*
*   @param[in] nidd_data base address of received data
*   @param[in] nidd_data_len received data length
*   @return void
*          
*/

static void jiot_nidd_plat_event_callbacks(jiot_nidd_plat_event_e event, void *nidd_data, uint16_t nidd_data_len)
{
    JIOT_NIDD_LOG_D("entry : %s nidd_event_handler, event %d", __func__, event);
    switch (event)
    {
        case E_NIDD_PLAT_EVENT_CHANNEL_ACTIVATE_IND:
            JIOT_NIDD_LOG_I("nidd channel activated !!!");
            break;

        case E_NIDD_PLAT_EVENT_CHANNEL_DEACTIVATE_IND:
            JIOT_NIDD_LOG_I("nidd channel deactivated !!!");
            break;

        case E_NIDD_PLAT_EVENT_DATA_IND:
			{

                jiot_nidd_plat_data_ind_t *recv_data = (jiot_nidd_plat_data_ind_t *)nidd_data;
                jiot_nidd_data_t *data = NULL;
	            data = (jiot_nidd_data_t *)jiot_nidd_utility_calloc(1, sizeof(jiot_nidd_data_t));
                if(data)
	            {
	                data->p_data = jiot_nidd_utility_calloc(recv_data->data_len+1, sizeof(uint8_t));
	                if(data->p_data)
	                {
                        jiot_nidd_osal_message_t msg = {0};
	                    memcpy((void *)data->p_data,(void *)recv_data->p_data, recv_data->data_len);
                        data->data_len = recv_data->data_len;
                        msg.message = (void *)data;
	                    msg.msgHandler = jiot_nidd_receive_data;
	                    jiot_nidd_osal_message_processor_send(nidd_msg_processor_handler, &msg);
                        
	                }
	                else
	                {
	                    jiot_nidd_utility_free(data);
	                    JIOT_NIDD_LOG_E("Memory Allocation for data is Failed");
	                }
	            }
	            else
	                JIOT_NIDD_LOG_E("Memory allocation : Failed");
        	}
            break;

        default:
            JIOT_NIDD_LOG_E("Invalid NIDD event");
            break;
    }
}

/*-----------------------------------------------------------------------------------------------*/
/**
* @brief This function generates the meta data which is required to be sent during device activation
*	@return char* containing metadata if success or NULL in case of failure . 
*/
char *jiot_nidd_get_dev_metaData(void)
{
    JIOT_NIDD_LOG_D("entry : %s",__func__);
    int length = 0;
	char *ret_buff = NULL;

    length = snprintf(NULL,0,JIOT_NIDD_METADATA_BODY_FMT,g_conf_make,g_conf_dm_domain,g_device_model,\
            g_conf_manufacturer,g_conf_rb_dmtype,g_conf_dmtype,g_conf_protocol,g_conf_c2d_channel,\
            g_conf_swversion,g_conf_fwversion,g_conf_os_version)+1;

    ret_buff = (char *)jiot_nidd_utility_calloc(length,sizeof(char));
    if(!ret_buff)
    {
        JIOT_NIDD_LOG_E("Memory allocation for ret_buf : Failed");
        return NULL;
    }
    else
    {
        sprintf(ret_buff,JIOT_NIDD_METADATA_BODY_FMT,g_conf_make,g_conf_dm_domain,g_device_model,\
            g_conf_manufacturer,g_conf_rb_dmtype,g_conf_dmtype,g_conf_protocol,g_conf_c2d_channel,\
            g_conf_swversion,g_conf_fwversion,g_conf_os_version);

        JIOT_NIDD_LOG_D("Meta Data : %s",ret_buff);
        return ret_buff;
    }
}

/*-----------------------------------------------------------------------------------------------*/
/**
 * @brief This function generate the uplink Packet as per JPP_v1
 *   @param[in] header header information of the NIDD message
 *   @param[in] topic topic for D2C message
 *   @param[in] payload application payload for D2C message
 *   @param[in] payloadLen application payload length
 *   @param[in] msg_id message ID
 *   @param[in] length ptr which hold length of complete D2C payload as per JPP v1.
 *   @param[in] trans_id transaction ID.
 *   @return - Uplink Payload as per JPP_v1. Memory is allocated by callee and freed by caller.
 * 
 */
uint8_t * jiot_nidd_prepare_packet(jiot_nidd_cmn_header_t *header, char *topic, uint8_t *payload,\
           uint16_t payloadLen, uint16_t *msg_id, uint16_t *length, char *trans_id)
{
    JIOT_NIDD_LOG_D("entry : %s ",__func__);
    uint16_t topicNameLen = 0;
    uint16_t uplink_len = 0;
    static uint16_t msgidentifier = 1;
    uint8_t *uplink_data = NULL;

    uplink_len = sizeof(jiot_nidd_cmn_header_t);

    if((header->pkt_type == E_NIDD_PKT_PUBCON) || (header->msg_type == E_NIDD_MSG_PUBLISH) || (header->msg_type == E_NIDD_MSG_METADATA))
    {
        uplink_len += JIOT_NIDD_TPC_PLD_MSGID_LNGTH_FLD;
    }  
    else if(header->pkt_type == E_NIDD_PKT_PUBACK)
    {
        uplink_len += JIOT_NIDD_TRANS_ID_LEN; 
    }
        

    if(topic)
	{
		topicNameLen = strlen(topic);
		uplink_len += JIOT_NIDD_TPC_PLD_MSGID_LNGTH_FLD;
		uplink_len += topicNameLen;
	}

	if(payload)
	{
		uplink_len += JIOT_NIDD_TPC_PLD_MSGID_LNGTH_FLD;
		uplink_len += payloadLen;
	}

    uplink_data = jiot_nidd_utility_calloc(uplink_len + 1, sizeof(uint8_t));
    if(!uplink_data)
    {
        JIOT_NIDD_LOG_E("Memory Allocation for uplink_data : Failed");
        *length = 0;
        *msg_id = 0;
        return NULL;
    }

    uplink_len = sizeof(jiot_nidd_cmn_header_t);
	memcpy((void *)uplink_data,(void *)header,uplink_len);

	if((header->pkt_type == E_NIDD_PKT_PUBCON) || (header->msg_type != E_NIDD_MSG_PUBLISH))
	{
		if(msgidentifier >= 0xFFFF)
			msgidentifier = 1;

		*msg_id = msgidentifier++;
		uplink_data[uplink_len++] =  *msg_id >> 8;
		uplink_data[uplink_len++] =  *msg_id & 0x00FF;
	}
	else if(header->pkt_type == E_NIDD_PKT_PUBACK)
	{
		memcpy((void *)&uplink_data[uplink_len],(void *)trans_id,JIOT_NIDD_TRANS_ID_LEN);
		uplink_len += JIOT_NIDD_TRANS_ID_LEN;
	}

	if(topic)
	{
		uplink_data[uplink_len++] = topicNameLen >> 8;
		uplink_data[uplink_len++] = topicNameLen & 0x00FF;
		memcpy((void *)&uplink_data[uplink_len],(void *)topic,topicNameLen);
		uplink_len += topicNameLen;
	}

	if(payload)
	{
		uplink_data[uplink_len++] = payloadLen >> 8;
		uplink_data[uplink_len++] = payloadLen & 0x00FF;
		memcpy((void *)&uplink_data[uplink_len],(void *)payload,payloadLen);
		uplink_len += payloadLen;
	}
	*length = uplink_len;
	JIOT_NIDD_LOG_D("Uplink length = %d",uplink_len);
	return uplink_data;
}

/*-----------------------------------------------------------------------------------------------*/
/**
* @brief  This function activates the application by contacting JNOPS server and fills in a structure jiot_nidd_app_params_t.
*   @param[in] appName Apllication Name
*   @param[in] plmid plmid received during default app activation.
*   @param[in] header header information of the NIDD message
*   @return jiot_nidd_error_code_e
*/
jiot_nidd_error_code_e jiot_nidd_activate_specific_app( char *appName, jiot_nidd_cmn_header_t *header, jiot_nidd_app_params_t * app_params)
{
    JIOT_NIDD_LOG_D("entry : %s",__func__);
	int length = 0;
	uint8_t *uplink_data = NULL;
	int retVal = E_NIDD_ERROR_FAILURE;
	uint8_t *data = NULL;
	uint16_t data_len = 0;
	uint16_t msg_id = 0;

    if(app_params->plmid == NULL)
    {
        JIOT_NIDD_LOG_E("plmid is NULL");
        return retVal;
    }
        

	length = snprintf(NULL,0,JIOT_NIDD_ACT_APP_REQ_FMT,API_VERSION,appName,app_params->plmid);
	uplink_data = jiot_nidd_utility_calloc(length + 1,sizeof(uint8_t));     /* uplink_data : appname + plmid*/
	if(!uplink_data)
	{
		JIOT_NIDD_LOG_D("Memory Allocation for uplink_data is failed");
		retVal =  E_NIDD_ERROR_NO_MEMORY;
		goto memfree;
	}
	sprintf(uplink_data,JIOT_NIDD_ACT_APP_REQ_FMT,API_VERSION,appName,app_params->plmid);
	JIOT_NIDD_LOG_D("uplink_data = %s",uplink_data);


	data = jiot_nidd_prepare_packet(header,NULL,uplink_data,length,&msg_id,&data_len,NULL);     /*data : header +appname + plmid*/
	if(!data)
	{
		retVal =  E_NIDD_ERROR_NO_MEMORY;
		goto memfree;
	}

    jiot_nidd_dumphex((void *)data,data_len);

    retVal = jiot_nidd_plat_send_data(jiot_nidd_id,(void *)data,data_len);
    if(retVal != E_NIDD_PLAT_RET_OK)
    {
        JIOT_NIDD_LOG_E("uplink send : Failed");
        retVal = E_NIDD_ERROR_FAILURE;
    }
    else
    {
        JIOT_NIDD_LOG_I("uplink send : Success");
        retVal = jiot_nidd_add_msgid_to_list(msg_id, (void *)app_params);
        if(retVal != E_NIDD_SUCCESS)
        {
            goto memfree;
        }

    }

	if((jiot_nidd_osal_semaphore_wait(nidd_sem_handler,60000) == 0) && (app_params->svc != NULL) && (app_params->eid != NULL)\
        && (app_params->vid != NULL) && (app_params->aid != NULL) && (app_params->appName != NULL))
    {
        retVal = E_NIDD_SUCCESS;
    }	
	else
	{
		retVal = E_NIDD_ERROR_FAILURE;
	}
    
    memfree:
        if(uplink_data)
	        jiot_nidd_utility_free((void *)uplink_data);
        if(data)
            jiot_nidd_utility_free(data);

	return retVal;
}

/*-----------------------------------------------------------------------------------------------*/
/**
*   This function activate the device by contacting JNOPS server and fills in the default app details in a struct jiot_nidd_app_params_t.
*   @param[in] header - header information of the NIDD message
*   @return - on success it will return JIOT_NIDD_SUCCESS while on Failure it return jiot_nidd_error_code_e
*/
jiot_nidd_error_code_e jiot_nidd_activate_default_app(jiot_nidd_cmn_header_t *header, jiot_nidd_app_params_t * app_params)
{
    JIOT_NIDD_LOG_D("entry : %s",__func__);
    char* meta_data = NULL;
    uint8_t* uplink_data = NULL;
    int length = 0;
    int retVal = E_NIDD_ERROR_FAILURE;
    uint8_t *data = NULL;
    uint16_t data_len = 0;
    uint16_t msg_id = 0;
    char *dev_uid = NULL;

    meta_data = jiot_nidd_get_dev_metaData();
    if(!meta_data)
    {
        JIOT_NIDD_LOG_E("Meta Data is NULL");
        goto memfree;
    }
    
    if(jiot_nidd_conf_gen_create_file() == false)
    JIOT_NIDD_LOG_E("Config Gen File Create Failed");
    
    jiot_nidd_get_dev_uniqueId(&dev_uid);

    length = snprintf(NULL,0,JIOT_NIDD_ACT_DEV_REQ_FMT,API_VERSION,dev_uid,meta_data); /* Uplink data : device_uid + meta_data */
    uplink_data = (uint8_t *)jiot_nidd_utility_calloc(length + 1, sizeof(uint8_t));
    if(!uplink_data)
    {
        JIOT_NIDD_LOG_E("Memory allocation for Uplink_data : Failed");
        retVal = E_NIDD_ERROR_NO_MEMORY;
        goto memfree;
    }

    sprintf(uplink_data,JIOT_NIDD_ACT_DEV_REQ_FMT,API_VERSION,dev_uid,meta_data);
    JIOT_NIDD_LOG_D("Device Act FMT : %s",uplink_data);

    data = jiot_nidd_prepare_packet(header,NULL,uplink_data,length,&msg_id,&data_len,NULL); /*data : header + device_uid + meta_data*/
    if(!data)
    {
        JIOT_NIDD_LOG_E("Memory allocation for data failed");
        retVal = E_NIDD_ERROR_NO_MEMORY;
        goto memfree;
    }
    
    jiot_nidd_dumphex((void *)data,data_len);
    
    retVal = jiot_nidd_plat_send_data(jiot_nidd_id,(void *)data,data_len);
    if(retVal != E_NIDD_PLAT_RET_OK)
    {
        JIOT_NIDD_LOG_E("uplink send : Failed");
        retVal = E_NIDD_ERROR_FAILURE;
        goto memfree;
    }
    else
    {
        JIOT_NIDD_LOG_I("uplink send : Success");
        retVal = jiot_nidd_add_msgid_to_list(msg_id, (void *)app_params);
        if(retVal != E_NIDD_SUCCESS)
        {
            goto memfree;
        }

    }

    if((jiot_nidd_osal_semaphore_wait(nidd_sem_handler,60000) == 0) && (app_params->plmid != NULL))
    {
        retVal = E_NIDD_SUCCESS;
    }		
	else
	{
		retVal = E_NIDD_ERROR_FAILURE;
	}

    memfree:
    if(dev_uid)
        jiot_nidd_utility_free(dev_uid);
    if(meta_data)
        jiot_nidd_utility_free(meta_data);
    if(uplink_data)
        jiot_nidd_utility_free(uplink_data);
    if(data)
        jiot_nidd_utility_free(data);
   
    return retVal;

}

/*-----------------------------------------------------------------------------------------------*/
/**
* @brief  This function create NIDD message processor, Semaphore and Setup NIDD connection
*  @return - on success it will return JIOT_NIDD_SUCCESS while on Failure it return jiot_nidd_error_code_e
*
*/
jiot_nidd_error_code_e jiot_nidd_init(void){
    JIOT_NIDD_LOG_D("entry :%s",__func__);

    if(nidd_msg_processor_handler == NULL)
        nidd_msg_processor_handler = jiot_nidd_osal_message_processor_create(jiot_nidd_worker_start);

    if(nidd_sem_handler == NULL)
        nidd_sem_handler = jiot_nidd_osal_semaphore_create(0);

    if(jiot_nidd_plat_connect(&jiot_nidd_id, JIO_NWTK_APN, jiot_nidd_plat_event_callbacks) != E_NIDD_PLAT_RET_OK)
    {
        JIOT_NIDD_LOG_E("plat connect : Failed");
        is_init_done = false;
        return E_NIDD_ERROR_FAILURE;
    }
    is_init_done = true;
    return E_NIDD_SUCCESS;
}

/*-----------------------------------------------------------------------------------------------*/
/**
* @brief This function register the application by contacting JNOPS server and store the app/device details. 
*   @param[in] appName - Name of the app whose details are to be fetched from JNOPS 
*   @param[in] context - middleware Context. 
*   @param[in] cb_options -  application callback .
*   @return - on success it will return JIOT_NIDD_SUCCESS while on Failure it return jiot_nidd_error_code_e 
*/
jiot_nidd_error_code_e jiot_nidd_registration(char *appName, jiot_nidd_handle_t *context, jiot_nidd_callback* cb_options)
{
    JIOT_NIDD_LOG_D("entry :%s",__func__);

    int retVal = E_NIDD_ERROR_FAILURE;
    jiot_nidd_cmn_header_t header = {0};
    jiot_nidd_app_params_t *app_parameters = NULL;

    if( appName == NULL || context == NULL || cb_options == NULL)
    {
        JIOT_NIDD_LOG_E("Invalid param passed.");
        return E_NIDD_ERROR_INVALID_PARAM;
    }
    
    if(!is_init_done)
    {
        retVal = jiot_nidd_init();
        if(retVal != E_NIDD_SUCCESS)
        {
            JIOT_NIDD_LOG_E("jiot_nidd_init : Failed");
            return retVal;
        }
    }
    
        header.protocol_discriminator = E_NIDD_PRTCL_JPP;
        header.version = JIOT_NIDD_JPP_VERSION;
        header.pkt_type = E_NIDD_PKT_PUBCON;	
        header.msg_type = E_NIDD_MSG_APPACT;
        header.content_type = E_NIDD_CNTNT_JSON;
        header.compr_type = E_NIDD_CMPR_PLD_UC;
        header.reserved = 0;

        app_parameters = (jiot_nidd_app_params_t *)jiot_nidd_utility_malloc(sizeof(jiot_nidd_app_params_t));
        if(!app_parameters)
        {
            JIOT_NIDD_LOG_E("Memory Allocation for app_parameters : Failed");
            return E_NIDD_ERROR_NO_MEMORY;
        }
        memset(app_parameters, 0x00, sizeof(jiot_nidd_app_params_t));

        is_dev_act = true;
        retVal = jiot_nidd_activate_default_app(&header, app_parameters);
        if(retVal != E_NIDD_SUCCESS)
        {
             if(app_parameters)
                jiot_nidd_utility_free(app_parameters);

            JIOT_NIDD_LOG_E("Device Activation Failed Ret value = %d",retVal);
            return E_NIDD_ERROR_FAILURE;
        }

        is_dev_act = false;
        retVal = jiot_nidd_activate_specific_app(appName, &header, app_parameters);
        if(retVal != E_NIDD_SUCCESS)
        {
            if(app_parameters->plmid)
                jiot_nidd_utility_free(app_parameters->plmid);
            if(app_parameters)
                jiot_nidd_utility_free(app_parameters);

            JIOT_NIDD_LOG_E("App Activation Failed Ret value = %d",retVal);
            return E_NIDD_ERROR_FAILURE;
        }
        app_parameters->cb_options = cb_options;

        if(app_parameters->plmid != NULL || app_parameters->appName != NULL || app_parameters->svc != NULL || app_parameters->eid != NULL\
            || app_parameters->vid != NULL || app_parameters->aid != NULL || app_parameters->cb_options != NULL)
        {
            retVal = jiot_nidd_add_app_params_to_list(app_parameters);
            if(retVal != E_NIDD_SUCCESS)
            {
                return E_NIDD_ERROR_FAILURE;
            }
            *context = (void *)app_parameters;
            return E_NIDD_SUCCESS;
        }
        else
        {
            JIOT_NIDD_LOG_E("some params of app_params is NULL");
            return E_NIDD_ERROR_INVALID_PARAM;
        }

    return E_NIDD_SUCCESS;
}

/*-----------------------------------------------------------------------------------------------*/
/**
* @brief This function generates the topic as per JPP V1 which is required to be sent during publish
*   @param[in] event target event type.
*   @param[in] svc application service code 
*   @param[in] device_plmid  platform ID.
*	@return char* containing topic if success or NULL in case of failure . Memory is allocated by callee and freed by caller.
*/
char * jiot_nidd_topic_formation(char *event, char *svc, char *device_plmid)
{
    JIOT_NIDD_LOG_D("entry : %s",__func__);
    int length;
    char *topic = NULL;

    if((event == NULL) || (svc == NULL) || (device_plmid == NULL))
    {
        JIOT_NIDD_LOG_E("Some Params are missing");
        return NULL;
    }

    length = snprintf(NULL,0,JIOT_NIDD_TOPIC_FMT,svc, DEVICE_TYPE, device_plmid, event) + 1;

    topic = (char *)jiot_nidd_utility_calloc(length, sizeof(char));
    if(!topic)
    {
        JIOT_NIDD_LOG_E("Memory Allocation for Topic is failed");
        return NULL;
    }
    else
    {
        sprintf(topic,JIOT_NIDD_TOPIC_FMT,svc, DEVICE_TYPE, device_plmid, event);
        JIOT_NIDD_LOG_D("Topic : %s",topic);
        return topic;
    }

}

/*-----------------------------------------------------------------------------------------------*/
/**
 * @brief This function generates the payload as per JPP V1 which is required to be sent during publish
 *   @param[in] app_params address of struct jiot_nidd_app_params_t
 *   @param[in] targetInfo target event type. address of struct jiot_nidd_target_info_t
 *   @param[in] timeStamp current timestamp.
 *	@return jiot_nidd_sendPayload_t* containing payload and payloadlen if success or NULL in case of failure . Memory is allocated by callee and freed by caller.
 */
jiot_nidd_sendPayload_t *jiot_nidd_sendPayload_formation(jiot_nidd_app_params_t *app_params, jiot_nidd_target_info_t *targetInfo, const jiot_nidd_data_t *appData)
{
    JIOT_NIDD_LOG_D("entry : %s", __func__);
    jiot_nidd_sendPayload_t *sendPayload_data = NULL;
    char *deviceType = g_conf_dmtype;
    char *evt = (strcmp(targetInfo->event, "alerts") == 0) ? "AL" : "EV";
    char *evtCode = (strcmp(targetInfo->event, "alerts") == 0) ? "alc" : "evc";
    char *tmsTime = jiot_nidd_get_curent_timeStamp();
    if (!tmsTime)
    {
        JIOT_NIDD_LOG_E("tmsTime is NULL");
        return NULL;
    }

    int payloadLength = snprintf(NULL, 0, "{\"ver\":\"1.0\",\"pld\":\"%s\",\"svc\":\"%s\",\"aid\":\"%s\",\"eid\":\"%s\",\"dvt\":\"%s\",\"dvm\":\"%s\",\"evt\":\"%s\",\"tms\":\"%s\",\"%s\":\"%d\",", app_params->plmid, app_params->svc, app_params->aid, app_params->eid, deviceType, DEVICEMODEL, evt, tmsTime, evtCode, targetInfo->evc);
    if (targetInfo->tid)
        payloadLength += snprintf(NULL, 0, "\"tid\":\"%s\",", targetInfo->tid);

    if (targetInfo->ahr)
    {
        payloadLength += snprintf(NULL, 0, "\"ahr\":%s,", targetInfo->ahr);
    }

    payloadLength += snprintf(NULL, 0, "\"ext\":");

    unsigned char *payLoadData = (unsigned char *)jiot_nidd_utility_calloc((payloadLength + appData->data_len + 2), sizeof(char));

    if (payLoadData == NULL)
    {
        JIOT_NIDD_LOG_E("Memory allocation failed for payloadData");
        jiot_nidd_utility_free(tmsTime);
        return NULL;
    }
    payloadLength = sprintf(payLoadData, "{\"ver\":\"1.0\",\"pld\":\"%s\",\"svc\":\"%s\",\"aid\":\"%s\",\"eid\":\"%s\",\"dvt\":\"%s\",\"dvm\":\"%s\",\"evt\":\"%s\",\"tms\":\"%s\",\"%s\":\"%d\",", app_params->plmid, app_params->svc, app_params->aid, app_params->eid, deviceType, DEVICEMODEL, evt, tmsTime, evtCode, targetInfo->evc);

    if (targetInfo->tid)
        payloadLength += sprintf(payLoadData + payloadLength, "\"tid\":\"%s\",", targetInfo->tid);

    if (targetInfo->ahr)
    {
        payloadLength += sprintf(payLoadData + payloadLength, "\"ahr\":%s,", targetInfo->ahr);
    }
    payloadLength += sprintf(payLoadData + payloadLength, "\"ext\":");

    memcpy(payLoadData + payloadLength, appData->p_data, appData->data_len);

    payloadLength += appData->data_len + 1;
    payLoadData[payloadLength - 1] = '}';
    payLoadData[payloadLength] = '\0';

    sendPayload_data = (jiot_nidd_sendPayload_t *)jiot_nidd_utility_malloc(sizeof(jiot_nidd_sendPayload_t));
    if (!sendPayload_data)
    {
        JIOT_NIDD_LOG_E("Memory Allocation jiot_nidd_sendPayload_t is Failed");
        jiot_nidd_utility_free(payLoadData);
        jiot_nidd_utility_free(tmsTime);
        return NULL;
    }
    memset(sendPayload_data, 0, sizeof(jiot_nidd_sendPayload_t));
    sendPayload_data->data = payLoadData;
    sendPayload_data->size = payloadLength;

    jiot_nidd_utility_free(tmsTime);
    return sendPayload_data;
}

/*-----------------------------------------------------------------------------------------------*/
/**
 * @brief  This function use to form the uplink packet as per JPP_V1 protocol.
 *  @param[in] nidd_parmas send parameter details
 *  @param[in] topic topic
 *  @param[in] payload application payload
 *  @param[in] payloadLen application payload length
 *  @return - on success it will return JIOT_NIDD_SUCCESS while on Failure it return jiot_nidd_error_code_e 
 * 
 */

jiot_nidd_error_code_e jiot_nidd_msg_send_formation(jiot_nidd_send_param_t *nidd_parmas, char *topic, uint8_t *payload,\
        uint16_t payloadLen, jiot_nidd_cntnt_type_e contentType)
{
    JIOT_NIDD_LOG_D("entry : %s",__func__);
    jiot_nidd_cmn_header_t header = {0};
    uint8_t *data = NULL;

	header.protocol_discriminator = E_NIDD_PRTCL_JPP;
	header.version = JIOT_NIDD_JPP_VERSION;
	header.pkt_type = E_NIDD_PKT_PUBCON;	
	header.msg_type = E_NIDD_MSG_PUBLISH;
	header.content_type = contentType;
	header.compr_type = E_NIDD_CMPR_TPC_UC_PLD_UC;
	header.reserved = 0;

    data = jiot_nidd_prepare_packet(&header, topic, payload, payloadLen, &nidd_parmas->msgid, (uint16_t *)&nidd_parmas->payload_len, NULL);
    if(!data)
    {
        nidd_parmas->payload_len = 0;
        nidd_parmas->payload_msg = NULL;
        return E_NIDD_ERROR_NO_MEMORY;
    }
    nidd_parmas->payload_msg = data;
    return E_NIDD_SUCCESS;
}

/*-----------------------------------------------------------------------------------------------*/
/**
*  @brief This handler used to send data over NIDD bearer
*   @param[in] appContext - Application Context
*   @param[in] targetInfo -  ptr of jiot_nidd_target_info_t
*   @param[in] appData - ptr of jiot_nidd_data_t
*   @param[in] deliveryMode - best effort delivery mode/guaranteed delivery mode. 
*   @param[in] msgId - identity of the message.
*   @return jiot_nidd_error_code_e
*
*/
void jiot_nidd_send_data(void *arg)
{
    JIOT_NIDD_LOG_D("entry : %s",__func__);
    int retVal = E_NIDD_PLAT_RET_ERROR;
    jiot_nidd_send_param_t *nidd_send_param = (jiot_nidd_send_param_t *)arg;
    jiot_nidd_app_params_t *app_params = NULL;
    char msg_id_buf [10];

    jiot_nidd_dumphex(nidd_send_param->payload_msg,nidd_send_param->payload_len);

    retVal = jiot_nidd_plat_send_data(jiot_nidd_id,nidd_send_param->payload_msg,nidd_send_param->payload_len);
    if(retVal == E_NIDD_PLAT_RET_OK)
    {
        retVal = jiot_nidd_add_msgid_to_list(nidd_send_param->msgid, (void *)nidd_send_param->appContext); 
        if(retVal != E_NIDD_SUCCESS)
        {
            JIOT_NIDD_LOG_E("Add msgID : Failed");
            goto memfree;
        }
    }
    else
    {
        JIOT_NIDD_LOG_E("uplink send : Failed");
        app_params = (jiot_nidd_app_params_t *)nidd_send_param->appContext;
        snprintf(msg_id_buf,10,"%d",nidd_send_param->msgid);
        app_params->cb_options(E_NIDD_SEND_FAILED, &msg_id_buf, strlen(msg_id_buf));
    }
    
    memfree:
    
    if(nidd_send_param)
    {
        if(nidd_send_param->payload_msg)
            jiot_nidd_utility_free(nidd_send_param->payload_msg);
        jiot_nidd_utility_free(nidd_send_param);
    }
        
    nidd_send_param = NULL;
}

/*-----------------------------------------------------------------------------------------------*/
/**
*  @brief This handler used to send data over NIDD bearer
*   @param[in] appContext - Application Context
*   @param[in] targetInfo -  ptr of jiot_nidd_target_info_t
*   @param[in] appData - ptr of jiot_nidd_data_t
*   @param[in] deliveryMode - best effort delivery mode/guaranteed delivery mode. 
*   @param[in] msgId - identity of the message.
*   @return jiot_nidd_error_code_e
*
*/
jiot_nidd_error_code_e jiot_nidd_send(jiot_nidd_handle_t context, jiot_nidd_target_info_t* targetInfo, const jiot_nidd_data_t *appData,
                                        jiot_nidd_delivery_mode_e deliveryMode, int *msgId)
{
    JIOT_NIDD_LOG_D("entry : %s",__func__);
   
    if((context == NULL) || (targetInfo == NULL) || (appData == NULL))
    {
        JIOT_NIDD_LOG_E("some params are missing");
        return E_NIDD_ERROR_INVALID_PARAM;
    }

    int retVal = E_NIDD_ERROR_FAILURE;
    char *topic = NULL;
    jiot_nidd_sendPayload_t *sendPayload = NULL;
    jiot_nidd_app_params_t *app_params = NULL;
    jiot_nidd_send_param_t *nidd_send_param = NULL;
    jiot_nidd_osal_message_t msg = {0};
    

    app_params = (jiot_nidd_app_params_t *)context;
    
    topic = jiot_nidd_topic_formation(targetInfo->event, app_params->svc, app_params->plmid);
    if(!topic){
        JIOT_NIDD_LOG_E("topic is NULL");
        retVal = E_NIDD_ERROR_NO_MEMORY;
        goto memfree;
    }

    sendPayload = jiot_nidd_sendPayload_formation(app_params, targetInfo, appData);
    if (sendPayload == NULL)
    {
        JIOT_NIDD_LOG_E("sendPayload is NULL");
        retVal = E_NIDD_ERROR_NO_MEMORY;
        goto memfree;
    }

    nidd_send_param = (jiot_nidd_send_param_t *)jiot_nidd_utility_malloc(sizeof(jiot_nidd_send_param_t));
    if(!nidd_send_param)
    {
        JIOT_NIDD_LOG_E("Memory Allocation for nidd_send_param is failed");
        retVal = E_NIDD_ERROR_NO_MEMORY;
        goto memfree;
    }
    memset(nidd_send_param, 0x00, sizeof(jiot_nidd_send_param_t));

    nidd_send_param->Mode = deliveryMode;
    nidd_send_param->msgid = *msgId;
    nidd_send_param->appContext = (void *)app_params->cb_options;

    retVal = jiot_nidd_msg_send_formation(nidd_send_param,topic,(uint8_t *)sendPayload->data,sendPayload->size, E_NIDD_CNTNT_JSON);
    if(retVal != E_NIDD_SUCCESS)
    {
        JIOT_NIDD_LOG_E("msg_send_formation is failed");
        *msgId = 0;
        jiot_nidd_utility_free(nidd_send_param);
        goto memfree;
    }
    msg.message = (void *)nidd_send_param;
    msg.msgHandler = jiot_nidd_send_data;
    jiot_nidd_osal_message_processor_send(nidd_msg_processor_handler, &msg);

     memfree:
        if(topic)
            jiot_nidd_utility_free(topic);
        if (sendPayload)
        {
            if (sendPayload->data)
                jiot_nidd_utility_free(sendPayload->data);
            jiot_nidd_utility_free(sendPayload);
        }
            
    return retVal;
}

/*-----------------------------------------------------------------------------------------------*/
/**
 *  @brief This api is used by application to send out its data to the server, NaaS will not add any meta data header in payload.
 *
 *   @param[in] context - Context of middleware.
 *   @param[in] targetInfo -  ptr of jiot_nidd_target_info_t. (Memory of struct and it's element is allocate and free by caller).
 *   @param[in] appData - ptr of jiot_nidd_data_t. (Memory of struct and it's element is allocate and free by caller).
 *   @param[in] deliveryMode - Mode in which the data has to be sent to server
 *   @param[in] contentType - content type of the appData
 *   @param[out] msgId - ID to identify the message sent
 *   @return jiot_nidd_error_code_e
 *
 */
jiot_nidd_error_code_e jiot_nidd_app_send(jiot_nidd_handle_t context, jiot_nidd_target_info_t *targetInfo, const jiot_nidd_data_t *appData,
                                          jiot_nidd_delivery_mode_e deliveryMode, jiot_nidd_cntnt_type_e contentType, int *msgId)
{
    JIOT_NIDD_LOG_D("entry : %s", __func__);

    if ((context == NULL) || (targetInfo == NULL) || (appData == NULL))
    {
        JIOT_NIDD_LOG_E("some params are missing");
        return E_NIDD_ERROR_INVALID_PARAM;
    }

    int retVal = E_NIDD_ERROR_FAILURE;
    char *topic = NULL;
    jiot_nidd_app_params_t *app_params = NULL;
    jiot_nidd_send_param_t *nidd_send_param = NULL;
    jiot_nidd_osal_message_t msg = {0};

    app_params = (jiot_nidd_app_params_t *)context;
    topic = jiot_nidd_topic_formation(targetInfo->event, app_params->svc, app_params->plmid);
    if (!topic)
    {
        JIOT_NIDD_LOG_E("topic is NULL");
        retVal = E_NIDD_ERROR_NO_MEMORY;
        goto memfree;
    }

    nidd_send_param = (jiot_nidd_send_param_t *)jiot_nidd_utility_malloc(sizeof(jiot_nidd_send_param_t));
    if (!nidd_send_param)
    {
        JIOT_NIDD_LOG_E("Memory Allocation for nidd_send_param is failed");
        retVal = E_NIDD_ERROR_NO_MEMORY;
        goto memfree;
    }
    memset(nidd_send_param, 0x00, sizeof(jiot_nidd_send_param_t));

    nidd_send_param->Mode = deliveryMode;
    nidd_send_param->msgid = *msgId;
    nidd_send_param->appContext = (void *)app_params->cb_options;

    retVal = jiot_nidd_msg_send_formation(nidd_send_param, topic, appData->p_data, appData->data_len, contentType);
    if (retVal != E_NIDD_SUCCESS)
    {
        JIOT_NIDD_LOG_E("msg_send_formation is failed");
        *msgId = 0;
        jiot_nidd_utility_free(nidd_send_param);
        goto memfree;
    }
    msg.message = (void *)nidd_send_param;
    msg.msgHandler = jiot_nidd_send_data;
    jiot_nidd_osal_message_processor_send(nidd_msg_processor_handler, &msg);

memfree:
    if (topic)
        jiot_nidd_utility_free(topic);

    return retVal;
}

/*-----------------------------------------------------------------------------------------------*/
/**
*   This function sends the ack for the C2D message received
*   @param[in] trans_id - Transaction ID of C2D message received
*   @param[in] header - Header details for the NIDD message to be sent
*   @return void
*
*/
void jiot_nidd_send_c2d_ack(char *trans_id, jiot_nidd_cmn_header_t *header)
{
    JIOT_NIDD_LOG_D("entry : %s",__func__);
	uint16_t msg_id = 0;
	uint8_t *c2d_data = NULL;
	uint16_t length = 0;
	int retVal = E_NIDD_ERROR_FAILURE;

	header->pkt_type = E_NIDD_PKT_PUBACK;

	c2d_data = jiot_nidd_prepare_packet(header , NULL, NULL, 0 ,&msg_id , &length ,trans_id);
	if(c2d_data != NULL)
    {
        jiot_nidd_dumphex((void *)c2d_data,length);

        retVal = jiot_nidd_plat_send_data(jiot_nidd_id, (void *)c2d_data, length);
	    if (retVal != E_NIDD_PLAT_RET_OK)
        {
            JIOT_NIDD_LOG_E("Sending C2D Ack  : Failed");
        }
        else
        {
            JIOT_NIDD_LOG_I("Sending C2D Ack : Success");
        }
    }
    else
    {
        JIOT_NIDD_LOG_E("data is NULL");
    }

    if(c2d_data)
        jiot_nidd_utility_free(c2d_data);

}

/*-----------------------------------------------------------------------------------------------*/
/**
 * @brief   This function will get the payload meta data
 *   @param[in] context - Middleware Context.
 *   @return On success return jiot_nidd_meta_data_t otherwise NULL
 *
 */
jiot_nidd_meta_data_t *jiot_nidd_get_payload_meta_data(jiot_nidd_handle_t context)
{
    if (context == NULL)
        return NULL;

    jiot_nidd_app_params_t *app_params = NULL;
    app_params = (jiot_nidd_app_params_t *)context;
    if(!app_params->app_metaData)
    {
        app_params->app_metaData = (jiot_nidd_meta_data_t *)jiot_nidd_utility_malloc(sizeof(jiot_nidd_meta_data_t));
        if (!app_params->app_metaData)
        {
            JIOT_NIDD_LOG_E("Memory Allocation for jiot_nidd_meta_data_t is failed");
            return NULL;
        }
        memset(app_params->app_metaData, 0x00, sizeof(jiot_nidd_meta_data_t));
        app_params->app_metaData->pld = app_params->plmid;
        app_params->app_metaData->aid = app_params->aid;
        app_params->app_metaData->svc = app_params->svc;
        app_params->app_metaData->eid = app_params->eid;
        app_params->app_metaData->vid = app_params->vid;
    }
    return app_params->app_metaData;
}

/*-----------------------------------------------------------------------------------------------*/
/**
*   This function activate the device by contacting JNOPS server and fills in the default app details in a struct jiot_nidd_app_params_t.
*   @param[in] header - header information of the NIDD message
*   @return - on success it will return JIOT_NIDD_SUCCESS while on Failure it return jiot_nidd_error_code_e
*/
jiot_nidd_error_code_e jiot_nidd_meta_data_formation(jiot_nidd_cmn_header_t *header, jiot_nidd_app_params_t * app_params)
{
    char* meta_data = NULL;
    uint8_t* uplink_data = NULL;
    int length = 0;
    int retVal = E_NIDD_ERROR_FAILURE;
    uint8_t *data = NULL;
    uint16_t data_len = 0;
    uint16_t msg_id = 0;
    char *dev_uid = NULL;
    jiot_nidd_meta_data_t *mymetaData = NULL;
    int RetVal = 0;

    RetVal = jiot_nidd_readconfigfile();
    if(RetVal == E_NIDD_CONFIG_FAILURE)
    {
        JIOT_NIDD_LOG_E("Read Params from Config file : Failed");
    }

    meta_data = jiot_nidd_get_dev_metaData();
    if(!meta_data)
    {
        JIOT_NIDD_LOG_E("Meta Data is NULL");
        goto memfree;
    }

    if(!app_params)
    {
        JIOT_NIDD_LOG_E("context is NULL");
        return E_NIDD_ERROR_INVALID_PARAM;
    }
    mymetaData = jiot_nidd_get_payload_meta_data(app_params);

    length = snprintf(NULL,0,JIOT_NIDD_META_DATA_REQ_FMT,API_VERSION,mymetaData->pld,meta_data);  /*Uplink data : Pld + meta_data */
    uplink_data = (uint8_t *)jiot_nidd_utility_calloc(length + 1, sizeof(uint8_t));
    if(!uplink_data)
    {
        JIOT_NIDD_LOG_E("Memory allocation for Uplink_data : Failed");
        retVal = E_NIDD_ERROR_NO_MEMORY;
        goto memfree;
    }

    sprintf(uplink_data,JIOT_NIDD_META_DATA_REQ_FMT,API_VERSION,mymetaData->pld,meta_data);
    JIOT_NIDD_LOG_D("Device Meta Data FMT : %s",uplink_data);

    data = jiot_nidd_prepare_packet(header,NULL,uplink_data,length,&msg_id,&data_len,NULL);  /*data : header + Pld + meta_data*/
    if(!data)
    {
        JIOT_NIDD_LOG_E("Memory allocation for data failed");
        retVal = E_NIDD_ERROR_NO_MEMORY;
        goto memfree;
    }
    
    jiot_nidd_dumphex((void *)data,data_len);
    
    retVal = jiot_nidd_plat_send_data(jiot_nidd_id,(void *)data,data_len);
    if(retVal != E_NIDD_PLAT_RET_OK)
    {
        JIOT_NIDD_LOG_E("uplink send : Failed");
        retVal = E_NIDD_ERROR_FAILURE;
        goto memfree;
    }
    else
    {
        JIOT_NIDD_LOG_I("uplink send : Success");
        retVal = jiot_nidd_add_msgid_to_list(msg_id, (void *)app_params);
        if(retVal != E_NIDD_SUCCESS)
        {
            goto memfree;
        }
    }
        
    if(jiot_nidd_osal_semaphore_wait(nidd_sem_handler,60000) == 0)
    {
        retVal = E_NIDD_SUCCESS;
    }		
	else
	{
		retVal = E_NIDD_ERROR_FAILURE;
	}

    memfree:
    if(dev_uid)
        jiot_nidd_utility_free(dev_uid);
    if(meta_data)
        jiot_nidd_utility_free(meta_data);
    if(uplink_data)
        jiot_nidd_utility_free(uplink_data);
    if(data)
        jiot_nidd_utility_free(data);

    return retVal;
}

/*-----------------------------------------------------------------------------------------------*/
/**
*   @brief   This function will update device meta data to the server
*   @param[in] context - middleware Context. 
*   @return - on success it will return JIOT_NIDD_SUCCESS while on Failure it return jiot_nidd_error_code_e 
*/
jiot_nidd_error_code_e jiot_nidd_update_device_meta_data(jiot_nidd_handle_t context)
{
    JIOT_NIDD_LOG_D("entry : %s",__func__);
    int retVal = E_NIDD_ERROR_FAILURE;
    jiot_nidd_cmn_header_t header = {0};
    jiot_nidd_app_params_t *app_parameters = NULL;

    if(context == NULL)
    {
        JIOT_NIDD_LOG_E("Invalid param passed.");
        return E_NIDD_ERROR_INVALID_PARAM;
    }

    header.protocol_discriminator = E_NIDD_PRTCL_JPP;
    header.version = JIOT_NIDD_JPP_VERSION;
    header.pkt_type = E_NIDD_PKT_PUBCON;	
    header.msg_type = E_NIDD_MSG_METADATA;
    header.content_type = E_NIDD_CNTNT_JSON;
    header.compr_type = E_NIDD_CMPR_PLD_UC;
    header.reserved = 0;

    app_parameters = (jiot_nidd_app_params_t *)(context);
    
    retVal = jiot_nidd_meta_data_formation(&header, app_parameters);
    if(retVal != E_NIDD_SUCCESS)
    {
        JIOT_NIDD_LOG_E("Device Meta Data Update Failed Ret value %d",retVal);
        return E_NIDD_ERROR_FAILURE;
    }
    return E_NIDD_SUCCESS;
}
/*-----------------------------------------------------------------------------------------------*/
/**
* @brief  This handler destroys the NIDD session
*   @param[in] context - Middleware Context. 
*   @return jiot_nidd_error_code_e
*
*/
jiot_nidd_error_code_e jiot_nidd_deregister(jiot_nidd_handle_t *context)
{
    JIOT_NIDD_LOG_D("entry : %s",__func__);
    jiot_nidd_app_params_t *app_params =NULL;
    int retval = E_NIDD_ERROR_FAILURE;

    if(*context == NULL)
    {
        JIOT_NIDD_LOG_E("context is NULL");
        return E_NIDD_ERROR_INVALID_PARAM;
    }

    app_params = (jiot_nidd_app_params_t *)(*context);
    retval = jiot_nidd_remove_app_params_from_list(app_params);
    if(retval != E_NIDD_SUCCESS)
    {
        JIOT_NIDD_LOG_E("Deregister : Failure");
    }
    *context = NULL;
    return E_NIDD_SUCCESS;
}
/* ===============================FUNCTION END================================================== */
/* ===============================END OF FILE ================================================== */
