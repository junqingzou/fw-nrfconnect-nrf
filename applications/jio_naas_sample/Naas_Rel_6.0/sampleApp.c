#include <stdio.h>
#include <string.h>
#include "jiot_nidd_api.h"
#include "jiot_nidd_utils.h"
//#include "FreeRTOSConfig.h"
//#include "FreeRTOS.h"
//#include "task.h"
//#include "task_def.h"
//#include "jtos_jsu_log_api.h"
//#include "jtos_jsu_json_api.h"

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
static uint8_t bin_data[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x57, 0x6F, 0x72, 0x6C, 0x64};
static char TransId[JIOT_NIDD_TRANS_ID_LENGTH+1] = {0};

void jiot_nidd_osal_thread_sleep(int millisecond)
{
	vTaskDelay((TickType_t) (millisecond/portTICK_PERIOD_MS));
}

void display_info(void)
{
    JIOT_NIDD_LOG_D("=========================== API LEVEL TESTING ===========================");
    JIOT_NIDD_LOG_I("AT+JNIDD=TEST,1,1 /*Registration*/");
    JIOT_NIDD_LOG_I("AT+JNIDD=TEST,1,2 /* Get Meta Data*/");
    JIOT_NIDD_LOG_I("AT+JNIDD=TEST,1,3 /*send data*/");
    JIOT_NIDD_LOG_I("AT+JNIDD=TEST,1,4 /*send binary data */");
    JIOT_NIDD_LOG_I("AT+JNIDD=TEST,1,5 /*send plain data from app*/");
    JIOT_NIDD_LOG_I("AT+JNIDD=TEST,1,6 /*send json data from app*/");
    JIOT_NIDD_LOG_I("AT+JNIDD=TEST,1,7 /*send binary data from app*/");
    JIOT_NIDD_LOG_I("AT+JNIDD=TEST,1,8 /* Update device meta data*/");
    JIOT_NIDD_LOG_I("AT+JNIDD=TEST,1,9 /*Deregistration*/ ");
    JIOT_NIDD_LOG_D("=================   FLOW  LEVEL TESTING ==============================");
    JIOT_NIDD_LOG_I("AT+JNIDD=TEST,2,count /*count : No of iternation (Registration/send/Deregitration)*/ =================");
    JIOT_NIDD_LOG_I("AT+JNIDD=TEST,3,JNB3150 /*model : user can enter device model */ =================");
    JIOT_NIDD_LOG_I("AT+JNIDD=TEST,4,nidd /*c2d channel : user can enter c2d channel */ =================");
    JIOT_NIDD_LOG_I("AT+JNIDD=TEST,5,coap /*Protocol : user can enter Protocol */ =================");

}
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
          JIOT_NIDD_LOG_E("parse failed with Error Code : %d",parse_result);
          retval = E_NIDD_ERROR_FAILURE;
          goto memfree;
    }
    else{
        retval = E_NIDD_SUCCESS;
    }

    strncpy(TransId,Trans_id,strlen(Trans_id));
    JIOT_NIDD_LOG_I("TransId : %s", TransId);

    memfree:
    if(Trans_id)
        jiot_nidd_utility_free(Trans_id);
    return retval;
}
void nidd_callback(jiot_nidd_cb_events_e event, void *data, uint16_t data_len){
    JIOT_NIDD_LOG_D("=====================START : Application Callbacks=====================");
    JIOT_NIDD_LOG_D("inside : %s, event %d, data_len : %d", __func__, event,data_len);
    switch (event)
    {
        case E_NIDD_SEND_SUCCESS:
            JIOT_NIDD_LOG_D("Msg ID  : %s send  : SUCCESS ",data);
            break;

        case E_NIDD_SEND_FAILED:
            JIOT_NIDD_LOG_D("Msg ID  : %s send  : FAILED ",data);
            break;

        case E_NIDD_DATA:
            JIOT_NIDD_LOG_D("data recv : %s data length : %d",data, data_len);
            jiot_nidd_get_tid(data);
            break;

        default:
            JIOT_NIDD_LOG_E("Invalid callback event");
            break;
    }
    
    JIOT_NIDD_LOG_D("=====================END : Apllication Callbacks=====================");
}

jiot_nidd_app_error_code_e jiot_nidd_test_registartion(void){
    JIOT_NIDD_LOG_D("=====================START : Application Registration =====================");

    jiot_nidd_app_error_code_e retVal = E_NIDD_APP_ERROR_FAILURE;
    if(jiot_nidd_registration(APPNAME, &appContext, &nidd_callback) == E_NIDD_SUCCESS)
    {
        JIOT_NIDD_LOG_D("App Registation : Success");
        retVal = E_NIDD_APP_SUCCESS;
    }
    else
    {
        JIOT_NIDD_LOG_E("App Registation : Failed");
        retVal = E_NIDD_APP_ERROR_FAILURE;
    }

    JIOT_NIDD_LOG_D("=====================END : Application Registration =====================");
    return retVal;
}

jiot_nidd_app_error_code_e jiot_nidd_test_update_device_meta_data(void){
    JIOT_NIDD_LOG_D("=====================START : Device Meta Data Update =====================");

    jiot_nidd_app_error_code_e retVal = E_NIDD_APP_ERROR_FAILURE;
    if(jiot_nidd_update_device_meta_data(appContext) == E_NIDD_SUCCESS)
    {
        JIOT_NIDD_LOG_D("Meta Data Update Send : Success");
        retVal = E_NIDD_APP_SUCCESS;
    }
    else
    {
        JIOT_NIDD_LOG_E("Meta Data Update Send : Failed");
        retVal = E_NIDD_APP_ERROR_FAILURE;
    }

    JIOT_NIDD_LOG_D("=====================END : Device Meta Data Update =====================");
    return retVal;
}

jiot_nidd_app_error_code_e jiot_nidd_test_get_metaData(void){

    JIOT_NIDD_LOG_D("=====================START : Get MetaData =====================");
    jiot_nidd_meta_data_t *mymetaData = NULL;
    if (!appContext)
    {
        JIOT_NIDD_LOG_E("There is a Registration Failure/ Not Register yet");
        return E_NIDD_APP_ERROR_FAILURE;
    }
    mymetaData = jiot_nidd_get_payload_meta_data(appContext);
    if (mymetaData)
        JIOT_NIDD_LOG_D("pld : %s aid : %s svc : %s eid : %s vid : %s", mymetaData->pld, mymetaData->aid, mymetaData->svc, mymetaData->eid, mymetaData->vid);
    else
        JIOT_NIDD_LOG_W("jiot_nidd_meta_data_t is returning NULL");

    JIOT_NIDD_LOG_D("=====================END : Get MetaData =====================");
    /* free meta data */

    return E_NIDD_APP_SUCCESS;
}

jiot_nidd_app_error_code_e jiot_nidd_test_send(void){

    JIOT_NIDD_LOG_D("=====================START : Application Data Send =====================");
    jiot_nidd_app_error_code_e retVal = E_NIDD_APP_ERROR_FAILURE;
    jiot_nidd_target_info_t *targetHandler = NULL;
    jiot_nidd_data_t *appData = NULL;
    int msgId = 0;
    if(!appContext)
    {
        JIOT_NIDD_LOG_E("There is a Registration Failure/ Not Register yet");
        return E_NIDD_APP_ERROR_FAILURE;
    }
    else
    {
        targetHandler = (jiot_nidd_target_info_t *)jiot_nidd_utility_malloc(sizeof(jiot_nidd_target_info_t));
        if(!targetHandler)
        {
            JIOT_NIDD_LOG_D("Memory Allocation for targetHandler is failed ");
            retVal = E_NIDD_APP_ERROR_NO_MEMORY;
            goto memfree; 
        }
        memset(targetHandler,0,sizeof(jiot_nidd_target_info_t));

        targetHandler->event = (char *)jiot_nidd_utility_calloc(strlen(EVENT)+1,sizeof(char));
        if(!targetHandler->event){
            JIOT_NIDD_LOG_E("Memory allocation for targetHandler->event : Failed");
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
            JIOT_NIDD_LOG_E("Memory allocation for appData in sampleApp is Failed");
            retVal = E_NIDD_APP_ERROR_NO_MEMORY;
            goto memfree;
        }
        memset(appData,0,sizeof(jiot_nidd_data_t));

       appData->p_data = (uint8_t *)jiot_nidd_utility_calloc(strlen(plain_data) + 1, sizeof(char));
        if (!appData->p_data)
        {
            JIOT_NIDD_LOG_E("Memory Allocation for appData->p_data is failed");
            retVal = E_NIDD_APP_ERROR_NO_MEMORY;
            goto memfree;
        }
        strncpy(appData->p_data, plain_data, strlen(plain_data));
        appData->data_len = strlen(plain_data);

        if(jiot_nidd_send(appContext,targetHandler,appData,E_BEST_EFFORT_DELIVERY_MODE,&msgId) != E_NIDD_SUCCESS)
        {
            JIOT_NIDD_LOG_E("jiot_nidd_send : Failed");
            retVal = E_NIDD_APP_ERROR_FAILURE;
        }
        else
        {
            JIOT_NIDD_LOG_D("send uplink : Success");
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
       
    JIOT_NIDD_LOG_D("=====================END : Application Data Send =====================");
    return retVal;
}

jiot_nidd_app_error_code_e jiot_nidd_test_bin_send(void)
{
    JIOT_NIDD_LOG_D("=====================START : Application Data Send Binary =====================");
    jiot_nidd_app_error_code_e retVal = E_NIDD_APP_ERROR_FAILURE;
    jiot_nidd_target_info_t *targetHandler = NULL;
    jiot_nidd_data_t *appData = NULL;
    int msgId = 0;
    if (!appContext)
    {
        JIOT_NIDD_LOG_E("There is a Registration Failure/ Not Register yet");
        return E_NIDD_APP_ERROR_FAILURE;
    }
    else
    {
        targetHandler = (jiot_nidd_target_info_t *)jiot_nidd_utility_malloc(sizeof(jiot_nidd_target_info_t));
        if (!targetHandler)
        {
            JIOT_NIDD_LOG_D("Memory Allocation for targetHandler is failed ");
            retVal = E_NIDD_APP_ERROR_NO_MEMORY;
            goto memfree;
        }
        memset(targetHandler, 0, sizeof(jiot_nidd_target_info_t));

        targetHandler->event = (char *)jiot_nidd_utility_calloc(strlen(EVENT) + 1, sizeof(char));
        if (!targetHandler->event)
        {
            JIOT_NIDD_LOG_E("Memory allocation for targetHandler->event : Failed");
            retVal = E_NIDD_APP_ERROR_NO_MEMORY;
            goto memfree;
        }
        strncpy(targetHandler->event, EVENT, strlen(EVENT));
        targetHandler->evc = 61;

        targetHandler->tid = NULL;
        targetHandler->ahr = NULL;
 
        appData = (jiot_nidd_data_t *)jiot_nidd_utility_malloc(sizeof(jiot_nidd_data_t));
        if (!appData)
        {
            JIOT_NIDD_LOG_E("Memory allocation for appData in sampleApp is Failed");
            retVal = E_NIDD_APP_ERROR_NO_MEMORY;
            goto memfree;
        }
        memset(appData, 0, sizeof(jiot_nidd_data_t));

        int len = snprintf(NULL, 0, "{\"bin\":1,\"len\":%lu,\"pay\":", sizeof(bin_data));
        appData->data_len = len + sizeof(bin_data) + 1;
        appData->p_data = (uint8_t *)jiot_nidd_utility_malloc(appData->data_len + 1);
        if (appData->p_data == NULL)
        {
            JIOT_NIDD_LOG_E("appData.data : Failed to allocate memory");
            retVal = E_NIDD_APP_ERROR_NO_MEMORY;
            goto memfree;
        }
        memset(appData->p_data, 0, appData->data_len + 1);
        sprintf(appData->p_data, "{\"bin\":1,\"len\":%lu,\"pay\":", sizeof(bin_data));
        strncpy(appData->p_data + len, bin_data, sizeof(bin_data));
        *(appData->p_data + appData->data_len - 1) = '}';
       
        if (jiot_nidd_send(appContext, targetHandler, appData, E_BEST_EFFORT_DELIVERY_MODE, &msgId) != E_NIDD_SUCCESS)
        {
            JIOT_NIDD_LOG_E("jiot_nidd_send : Failed");
            retVal = E_NIDD_APP_ERROR_FAILURE;
        }
        else
        {
            JIOT_NIDD_LOG_D("send uplink : Success");
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

    JIOT_NIDD_LOG_D("=====================END : Application Data Send Binary =====================");
    return retVal;
}

jiot_nidd_app_error_code_e jiot_nidd_test_app_data_send(jiot_nidd_app_content_type_e appContentType)
{
    JIOT_NIDD_LOG_D("=====================START : Application data send =====================");
    jiot_nidd_app_error_code_e retVal = E_NIDD_APP_ERROR_FAILURE;
    jiot_nidd_target_info_t *targetHandler = NULL;
    jiot_nidd_data_t *appData = NULL;
    jiot_nidd_meta_data_t *mymetaData = NULL;
    char *app_json_data = NULL;
    int app_json_data_len = 0;
    int msgId = 0;

    if (!appContext)
    {
        JIOT_NIDD_LOG_E("There is a Registration Failure/ Not Register yet");
        return E_NIDD_APP_ERROR_FAILURE;
    }
    else
    {
        targetHandler = (jiot_nidd_target_info_t *)jiot_nidd_utility_malloc(sizeof(jiot_nidd_target_info_t));
        if (!targetHandler)
        {
            JIOT_NIDD_LOG_D("Memory Allocation for targetHandler is failed ");
            retVal = E_NIDD_APP_ERROR_NO_MEMORY;
            goto memfree;
        }
        memset(targetHandler, 0, sizeof(jiot_nidd_target_info_t));

        targetHandler->event = (char *)jiot_nidd_utility_calloc(strlen(EVENT) + 1, sizeof(char));
        if (!targetHandler->event)
        {
            JIOT_NIDD_LOG_E("Memory allocation for targetHandler->event : Failed");
            retVal = E_NIDD_APP_ERROR_NO_MEMORY;
            goto memfree;
        }
        strncpy(targetHandler->event, EVENT, strlen(EVENT));

        appData = (jiot_nidd_data_t *)jiot_nidd_utility_malloc(sizeof(jiot_nidd_data_t));
        if (!appData)
        {
            JIOT_NIDD_LOG_E("Memory allocation for appData in sampleApp is Failed");
            retVal = E_NIDD_APP_ERROR_NO_MEMORY;
            goto memfree;
        }
        memset(appData, 0, sizeof(jiot_nidd_data_t));

        if(appContentType == E_NIDD_PLAIN_APP_DATA)
        {
            appData->p_data = (uint8_t *)jiot_nidd_utility_calloc(strlen(plain_data) + 1, sizeof(char));
            if (!appData->p_data)
            {
                JIOT_NIDD_LOG_E("Memory Allocation for appData->p_data is failed");
                retVal = E_NIDD_APP_ERROR_NO_MEMORY;
                goto memfree;
            }
            strncpy(appData->p_data, plain_data, strlen(plain_data));
            appData->data_len = strlen(plain_data);

            if (jiot_nidd_app_send(appContext, targetHandler, appData, E_BEST_EFFORT_DELIVERY_MODE, E_NIDD_CNTNT_TEXT, &msgId) != E_NIDD_SUCCESS)
            {
                JIOT_NIDD_LOG_E("jiot_nidd_send : Failed");
                retVal = E_NIDD_APP_ERROR_FAILURE;
            }
            else
            {
                JIOT_NIDD_LOG_D("send uplink : Success");
                retVal = E_NIDD_APP_SUCCESS;
            }

        }
        else if(appContentType == E_NIDD_BINARY_APP_DATA)
        {
            appData->p_data = (uint8_t *)jiot_nidd_utility_calloc(sizeof(bin_data), sizeof(char));
            if (!appData->p_data)
            {
                JIOT_NIDD_LOG_E("Memory Allocation for appData->p_data is failed");
                retVal = E_NIDD_APP_ERROR_NO_MEMORY;
                goto memfree;
            }
            memcpy(appData->p_data, &bin_data, sizeof(bin_data));
            appData->data_len = sizeof(bin_data);

            if (jiot_nidd_app_send(appContext, targetHandler, appData, E_BEST_EFFORT_DELIVERY_MODE, E_NIDD_CNTNT_BINARY, &msgId) != E_NIDD_SUCCESS)
            {
                JIOT_NIDD_LOG_E("jiot_nidd_send : Failed");
                retVal = E_NIDD_APP_ERROR_FAILURE;
            }
            else
            {
                JIOT_NIDD_LOG_D("send uplink : Success");
                retVal = E_NIDD_APP_SUCCESS;
            }
        }
        else
        {
            mymetaData = jiot_nidd_get_payload_meta_data(appContext);
            if (!mymetaData)
            {
                JIOT_NIDD_LOG_E("mymetaData context is NULL");
                return E_NIDD_APP_ERROR_FAILURE;
            }
            app_json_data_len = snprintf(NULL, 0, TEST_JSON_FMT, mymetaData->pld, mymetaData->svc, mymetaData->aid, mymetaData->eid, mymetaData->vid, plain_data);

            appData->p_data = (uint8_t *)jiot_nidd_utility_calloc(app_json_data_len + 1, sizeof(char));
            if (!appData->p_data)
            {
                JIOT_NIDD_LOG_E("Memory Allocation for appData->p_data is failed");
                retVal = E_NIDD_APP_ERROR_NO_MEMORY;
                goto memfree;
            }
            sprintf(appData->p_data, TEST_JSON_FMT, mymetaData->pld, mymetaData->svc, mymetaData->aid, mymetaData->eid, mymetaData->vid, plain_data);

            appData->data_len = app_json_data_len;

            if (jiot_nidd_app_send(appContext, targetHandler, appData, E_BEST_EFFORT_DELIVERY_MODE, E_NIDD_CNTNT_JSON, &msgId) != E_NIDD_SUCCESS)
            {
                JIOT_NIDD_LOG_E("jiot_nidd_send : Failed");
                retVal = E_NIDD_APP_ERROR_FAILURE;
            }
            else
            {
                JIOT_NIDD_LOG_D("send uplink : Success");
                retVal = E_NIDD_APP_SUCCESS;
            }   
        }
    }

memfree:

        if (targetHandler)
        {
            if (targetHandler->event)
                jiot_nidd_utility_free(targetHandler->event);
            jiot_nidd_utility_free(targetHandler);
        }
        if (appData)
        {
            if (appData->p_data)
                jiot_nidd_utility_free(appData->p_data);
            jiot_nidd_utility_free(appData);
        }

    JIOT_NIDD_LOG_D("=====================START : Application data send =====================");
    return retVal;
}

jiot_nidd_app_error_code_e jiot_nidd_test_deregistartion(void){
    JIOT_NIDD_LOG_D("=====================START : Application Dergistration =====================");

    jiot_nidd_app_error_code_e retVal = E_NIDD_APP_ERROR_FAILURE;
    if (!appContext)
    {
        JIOT_NIDD_LOG_E("There is a Registration Failure/ Not Register yet");
        return E_NIDD_APP_ERROR_FAILURE;
    }
    else
    {
        if(jiot_nidd_deregister(&appContext) != E_NIDD_SUCCESS)
        {
            JIOT_NIDD_LOG_D("App Deregistation : Failed");
            retVal = E_NIDD_APP_ERROR_FAILURE;
        }
        else
        {
            JIOT_NIDD_LOG_D("App Deregistation : Success"); 
            retVal = E_NIDD_APP_SUCCESS;
            appContext = NULL;
        }
    }

    JIOT_NIDD_LOG_D("=====================END : Application Deregistration =====================");
    return retVal;
}

jiot_nidd_app_error_code_e jiot_nidd_test_flow(int count){
    JIOT_NIDD_LOG_D("=====================START : FLOW TEST =====================");

    jiot_nidd_app_error_code_e retVal = E_NIDD_APP_ERROR_FAILURE;
    for(int i=1; i<(count+1); i ++)
    {
        JIOT_NIDD_LOG_D("TEST Count  : %d",i);
        retVal = jiot_nidd_test_registartion();
        if(retVal != E_NIDD_APP_SUCCESS)
        {
            JIOT_NIDD_LOG_D("Registration : Failed");
            return E_NIDD_APP_ERROR_FAILURE;
        }
        retVal = jiot_nidd_test_send();
         if(retVal != E_NIDD_APP_SUCCESS)
        {
            JIOT_NIDD_LOG_D("send : Failed");
            return E_NIDD_APP_ERROR_FAILURE;
        }

        retVal = jiot_nidd_test_deregistartion();
          if(retVal != E_NIDD_APP_SUCCESS)
        {
            JIOT_NIDD_LOG_D("Deregistration : Failed");
            return E_NIDD_APP_ERROR_FAILURE;
        }
        
        JIOT_NIDD_LOG_D("waiting for 60 sec ");
        jiot_nidd_osal_thread_sleep(60000);
    }

    JIOT_NIDD_LOG_D("=====================END : FLOW TEST =====================");
    return retVal;
}

int jiot_nidd_test(int argc, char *argv[]){

    JIOT_NIDD_LOG_D("inside : %s",__func__);
    JIOT_NIDD_LOG_D("argv[0] : %s argv[1] : %s argv[2] : %s ",argv[0],argv[1],argv[2]);

    if(atoi(argv[1])==1)
    {
        if (atoi(argv[2]) == 1)
        {
            jiot_nidd_test_registartion();
        }
        else if (atoi(argv[2]) == 2)
        {
            jiot_nidd_test_get_metaData();
        }
        else if (atoi(argv[2]) == 3)
        {
            jiot_nidd_test_send();
        }
        else if (atoi(argv[2]) == 4)
        {
            jiot_nidd_test_bin_send();
        }
        else if (atoi(argv[2]) == 5)
        {
            jiot_nidd_test_app_data_send(E_NIDD_PLAIN_APP_DATA);
        }
        else if (atoi(argv[2]) == 6)
        {
            jiot_nidd_test_app_data_send(E_NIDD_JSON_APP_DATA);
        }
        else if (atoi(argv[2]) == 7)
        {
            jiot_nidd_test_app_data_send(E_NIDD_BINARY_APP_DATA);
        }
        else if (atoi(argv[2]) == 8)
        {
            jiot_nidd_test_update_device_meta_data();
        }
        else if (atoi(argv[2]) == 9)
        {
            jiot_nidd_test_deregistartion();
        }
        else
        {
            JIOT_NIDD_LOG_E("Invalid Command");
        }
    }
    else if(!strcmp(argv[1],"2"))
    {
        jiot_nidd_test_flow(atoi(argv[2]));
    }
    else if(!strcmp(argv[1],"INFO"))
    {
        display_info();
    }
    else if(!strcmp(argv[1],"3"))
    {
        jiot_nidd_conf_gen_set_device_model(argv[2]);
    }
    else if(!strcmp(argv[1],"4"))
    {
        jiot_nidd_conf_gen_set_c2d_channel(argv[2]);
    }
    else if(!strcmp(argv[1],"5"))
    {
        jiot_nidd_conf_gen_set_protocol(argv[2]);
    }
    else
    {
        JIOT_NIDD_LOG_W("Invaild cmd : +=<JNIDD><API/FLOW><MENU>");
    }
    return 0;
}