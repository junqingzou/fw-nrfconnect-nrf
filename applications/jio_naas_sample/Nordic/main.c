/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <nrf_modem_at.h>
#include "jiot_nidd_api.h"
#include "jiot_nidd_utils.h"

LOG_MODULE_REGISTER(main, CONFIG_NAAS_LOG_LEVEL);

#define APPNAME "RJIL_JioConnectedWorker"
#define EVENT "alerts"
#define TEST_JSON_FMT "{\"ver\":\"1.0\",\"pld\":\"%s\",\"svc\":\"%s\",\"aid\":\"%s\",\"eid\":\"%s\",\"vid\":\"%s\",\"app_data\":\"%s\"}"
#define JIOT_NIDD_TRANS_ID_LENGTH  12

/**
 * Enum represents the error codes for Sample Applications.
 */
typedef enum jiot_nidd_app_error_code {
    E_NIDD_APP_SUCCESS                  =  0,      /*!< no error */ 
    E_NIDD_APP_ERROR_FAILURE            = -1,      /*!<failed >*/
    E_NIDD_APP_ERROR_NO_MEMORY          = -2,      /*!< memory resource not available */
}jiot_nidd_app_error_code_e;

/**
 * Enum represents the content type for Sample Applications.
 */
typedef enum jiot_nidd_app_content_type {
    E_NIDD_PLAIN_APP_DATA                       =  0,      /*!< Plain text */ 
    E_NIDD_JSON_APP_DATA                        = -1,      /*!< JSON >*/
    E_NIDD_BINARY_APP_DATA                      = -2,      /*!< Binary */
}jiot_nidd_app_content_type_e;

static jiot_nidd_handle_t appContext = NULL;
static char *plain_data = "Hello_World";
static char TransId[JIOT_NIDD_TRANS_ID_LENGTH+1] = {0};

/*-----------------------------------------------------------------------------------------------*/
/**
* @brief  This function parses the tid information recieves as part of C2D message
*   @param[in] buff - data received from the server as a C2D message
*   @return jiot_nidd_error_code_e
*/
int jiot_nidd_get_tid(char *buff)
{
    jiot_nidd_json_err_e parse_result = {0};
    jiot_nidd_json_info_t info = {0};
    int retval = E_NIDD_ERROR_FAILURE;
    int payload_len = strlen(buff);
    char *Trans_id = NULL;

    parse_result = jiot_nidd_json_parse_get_value((char*)buff,payload_len,"tid",&Trans_id,&info);
    if(parse_result != E_NIDD_JSON_SUCCESS)
    {
          LOG_ERR("parse failed with Error Code : %d",parse_result);
          retval = E_NIDD_ERROR_FAILURE;
          goto memfree;
    }
    else{
        retval = E_NIDD_SUCCESS;
    }

    strncpy(TransId,Trans_id,strlen(Trans_id));
    LOG_INF("TransId : %s", TransId);

    memfree:
    if(Trans_id)
        jiot_nidd_utility_free(Trans_id);
    return retval;
}

void nidd_callback(jiot_nidd_cb_events_e event, void *data, uint16_t data_len){
    LOG_DBG("=====================START : Application Callbacks=====================");
    LOG_DBG("inside : %s, event %d, data_len : %d", __func__, event,data_len);
    switch (event)
    {
        case E_NIDD_SEND_SUCCESS:
            LOG_DBG("Msg ID  : %s send  : SUCCESS ", (char *)data);
            break;

        case E_NIDD_SEND_FAILED:
            LOG_DBG("Msg ID  : %s send  : FAILED ", (char *)data);
            break;

        case E_NIDD_DATA:
            LOG_DBG("data recv : %s data length : %d", (char *)data, data_len);
            jiot_nidd_get_tid(data);
            break;

        default:
            LOG_ERR("Invalid callback event");
            break;
    }
    
    LOG_DBG("=====================END : Apllication Callbacks=====================");
}

jiot_nidd_app_error_code_e jiot_nidd_test_registartion(void){
    LOG_DBG("=====================START : Application Registration =====================");

    jiot_nidd_app_error_code_e retVal = E_NIDD_APP_ERROR_FAILURE;
    if(jiot_nidd_registration(APPNAME, &appContext, &nidd_callback) == E_NIDD_SUCCESS)
    {
        LOG_DBG("App Registation : Success");
        retVal = E_NIDD_APP_SUCCESS;
    }
    else
    {
        LOG_ERR("App Registation : Failed");
        retVal = E_NIDD_APP_ERROR_FAILURE;
    }

    LOG_DBG("=====================END : Application Registration =====================");
    return retVal;
}

jiot_nidd_app_error_code_e jiot_nidd_test_send(void){

    LOG_DBG("=====================START : Application Data Send =====================");
    jiot_nidd_app_error_code_e retVal = E_NIDD_APP_ERROR_FAILURE;
    jiot_nidd_target_info_t *targetHandler = NULL;
    jiot_nidd_data_t *appData = NULL;
    int msgId = 0;
    if(!appContext)
    {
        LOG_ERR("There is a Registration Failure/ Not Register yet");
        return E_NIDD_APP_ERROR_FAILURE;
    }
    else
    {
        targetHandler = (jiot_nidd_target_info_t *)jiot_nidd_utility_malloc(sizeof(jiot_nidd_target_info_t));
        if(!targetHandler)
        {
            LOG_DBG("Memory Allocation for targetHandler is failed ");
            retVal = E_NIDD_APP_ERROR_NO_MEMORY;
            goto memfree; 
        }
        memset(targetHandler,0,sizeof(jiot_nidd_target_info_t));

        targetHandler->event = (char *)jiot_nidd_utility_calloc(strlen(EVENT)+1,sizeof(char));
        if(!targetHandler->event){
            LOG_ERR("Memory allocation for targetHandler->event : Failed");
            retVal = E_NIDD_APP_ERROR_NO_MEMORY;
            goto memfree;
        }
        strncpy(targetHandler->event,EVENT,strlen(EVENT));
        targetHandler->evc = 61;
        targetHandler->tid = NULL;
        targetHandler->ahr = NULL;

        appData = (jiot_nidd_data_t *)jiot_nidd_utility_malloc(sizeof(jiot_nidd_data_t));
        if(!appData)
        {
            LOG_ERR("Memory allocation for appData in sampleApp is Failed");
            retVal = E_NIDD_APP_ERROR_NO_MEMORY;
            goto memfree;
        }
        memset(appData,0,sizeof(jiot_nidd_data_t));

       appData->p_data = (uint8_t *)jiot_nidd_utility_calloc(strlen(plain_data) + 1, sizeof(char));
        if (!appData->p_data)
        {
            LOG_ERR("Memory Allocation for appData->p_data is failed");
            retVal = E_NIDD_APP_ERROR_NO_MEMORY;
            goto memfree;
        }
        strncpy(appData->p_data, plain_data, strlen(plain_data));
        appData->data_len = strlen(plain_data);

        if(jiot_nidd_send(appContext,targetHandler,appData,E_BEST_EFFORT_DELIVERY_MODE,&msgId) != E_NIDD_SUCCESS)
        {
            LOG_ERR("jiot_nidd_send : Failed");
            retVal = E_NIDD_APP_ERROR_FAILURE;
        }
        else
        {
            LOG_DBG("send uplink : Success");
            retVal = E_NIDD_APP_SUCCESS;
        }   
    }

    memfree:
    if (targetHandler)
    {
        if (targetHandler->event)
            jiot_nidd_utility_free(targetHandler->event);
        if (targetHandler->tid)
            jiot_nidd_utility_free(targetHandler->tid);
        if (targetHandler->ahr)
            jiot_nidd_utility_free(targetHandler->ahr);
        jiot_nidd_utility_free(targetHandler);
    }
    if (appData)
    {
        if (appData->p_data)
            jiot_nidd_utility_free(appData->p_data);
        jiot_nidd_utility_free(appData);
    }
       
    LOG_DBG("=====================END : Application Data Send =====================");
    return retVal;
}

jiot_nidd_app_error_code_e jiot_nidd_test_deregistartion(void){
    LOG_DBG("=====================START : Application Dergistration =====================");

    jiot_nidd_app_error_code_e retVal = E_NIDD_APP_ERROR_FAILURE;
    if (!appContext)
    {
        LOG_ERR("There is a Registration Failure/ Not Register yet");
        return E_NIDD_APP_ERROR_FAILURE;
    }
    else
    {
        if(jiot_nidd_deregister(&appContext) != E_NIDD_SUCCESS)
        {
            LOG_DBG("App Deregistation : Failed");
            retVal = E_NIDD_APP_ERROR_FAILURE;
        }
        else
        {
            LOG_DBG("App Deregistation : Success"); 
            retVal = E_NIDD_APP_SUCCESS;
            appContext = NULL;
        }
    }

    LOG_DBG("=====================END : Application Deregistration =====================");
    return retVal;
}

jiot_nidd_app_error_code_e jiot_nidd_test_flow(int count){
    LOG_DBG("=====================START : FLOW TEST =====================");

    jiot_nidd_app_error_code_e retVal = E_NIDD_APP_ERROR_FAILURE;
    for(int i=1; i<(count+1); i ++)
    {
        LOG_DBG("TEST Count  : %d",i);
        retVal = jiot_nidd_test_registartion();
        if(retVal != E_NIDD_APP_SUCCESS)
        {
            LOG_DBG("Registration : Failed");
            return E_NIDD_APP_ERROR_FAILURE;
        }
        retVal = jiot_nidd_test_send();
         if(retVal != E_NIDD_APP_SUCCESS)
        {
            LOG_DBG("send : Failed");
            return E_NIDD_APP_ERROR_FAILURE;
        }

        retVal = jiot_nidd_test_deregistartion();
          if(retVal != E_NIDD_APP_SUCCESS)
        {
            LOG_DBG("Deregistration : Failed");
            return E_NIDD_APP_ERROR_FAILURE;
        }
        
        LOG_DBG("waiting for 60 sec ");
        k_sleep(K_MSEC(60000));
    }

    LOG_DBG("=====================END : FLOW TEST =====================");
    return retVal;
}

int main(void)
{
	//jiot_nidd_app_error_code_e retVal;
	int retVal;

	LOG_INF("Jio NAAS test start");

	retVal = jiot_nidd_test_registartion();
	if(retVal != E_NIDD_APP_SUCCESS)
	{
		LOG_DBG("Registration : Failed");
		return E_NIDD_APP_ERROR_FAILURE;
	}
#if 0
	retVal = jiot_nidd_test_send();
	if(retVal != E_NIDD_APP_SUCCESS)
	{
		LOG_DBG("send : Failed");
		return E_NIDD_APP_ERROR_FAILURE;
	}
#endif
	retVal = jiot_nidd_test_deregistartion();
	if(retVal != E_NIDD_APP_SUCCESS)
	{
		LOG_DBG("Deregistration : Failed");
		return E_NIDD_APP_ERROR_FAILURE;
	}

	LOG_INF("Jio NAAS test end");
	return E_NIDD_APP_SUCCESS;
}
