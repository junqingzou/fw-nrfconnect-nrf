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
 ============================================================================================= */

/* ===============================FILE REVISION HISTORY============================================
  Date         Author(RIL EMail ID)      CLIM ID(if applicable)       Detail of changes
--------       --------------------      ----------------------       -----------------
31-05-2021            JPL                         NONE                        CREATED
18-08-2022            JPL                         NONE                        Added API to update device meta data
                                                                              Added API to update paramter of Config file for c2dChannel, Protocol and DeviceModel.
===============================ENDOF REVISION HISTORY============================================ */

/* ===============================ABOUT THIS FILE=============================================== */
/**
* @file 
* @brief API exposed by NIDD module (middleware which implements Jio NIDD server-interface protocol), to be used by clients. 
*/
/*  ===============================END ABOUT THIS FILE=========================================== */

#ifndef __JIOT_NIDD_API_H__
#define __JIOT_NIDD_API_H__

#ifdef __cplusplus
extern "C"{
#endif

/* ===============================INCLUDE START================================================= */
#include <stdint.h>
#include <stdbool.h>
/* ===============================INCLUDE END=================================================== */


/* ====================================DEFINE START================================================= */
#define JIOT_NIDD_JPP_VERSION 					    1

/* ====================================DEFINE END================================================= */

/* ====================================ENUM START================================================== */
/**
 * Enum represents the error codes in NIDD operation
 */
typedef enum jiot_nidd_error_code {
    E_NIDD_SUCCESS                  =  0,      /*!< no error */ 
    E_NIDD_ERROR_FAILURE            = -1,      /*!<failed >*/
    E_NIDD_ERROR_NO_MEMORY          = -2,      /*!< memory resource not available */
    E_NIDD_ERROR_INVALID_PARAM      = -3,      /*!< invalid parameter(s) passed to Library */
    E_NIDD_ERROR_NO_CONNECTION      = -4,      /*!< not connected to a network */
    E_NIDD_ERROR_BUSY               = -5,      /*!< other operation is in progress */
    E_NIDD_ERROR_TIMEOUT            = -6,      /*!< operation timed out */
    E_NIDD_ERROR_NO_SOCKET          = -7,      /*!< socket not available for use */
    E_NIDD_ERROR_INVALID_APPNAME    = -8,      /*!< Invalid appName>*/
    E_NIDD_ERROR_INVALID_APN        = -9,      /*!< invalid apn */
}jiot_nidd_error_code_e;

/**
 * Enum represents protocol discriminator field in NIDD header
 */
typedef enum jiot_nidd_prtcl_dscrmntr {
    E_NIDD_PRTCL_JPP     =  0x00,            /*!< Jiothings NIDD Protocol */
    E_NIDD_PRTCL_COAP    =  0x01,            /*!< COAP Protocol */         
    E_NIDD_PRTCL_LWM2M   =  0x02,            /*!< LWM2M Protocol */
    E_NIDD_PRTCL_MQTT    =  0x03,            /*!< MQTT Protocol */
    E_NIDD_PRTCL_HTTP    =  0x04,            /*!< HTTP Protocol */         
    E_NIDD_PRTCL_RSRVD   =  0x05             /*!< Reserved*/
}jiot_nidd_prtcl_dscrmntr_e;

/**
 * Enum represents packet type field in NIDD header
 */
typedef enum jiot_nidd_pkt_type {
    E_NIDD_PKT_PUBNON  =  0x00,            /*!< Non Confirmable packet */
    E_NIDD_PKT_PUBCON  =  0x01,            /*!< Confirmable packet*/
    E_NIDD_PKT_PUBACK  =  0x02            /*!< Acknowledgement packet*/         
}jiot_nidd_pkt_type_e;


/**
 * Enum represents message type field in NIDD header
 */
typedef enum jiot_nidd_msg_type {
    E_NIDD_MSG_PUBLISH       =  0x00,            /*!< Publish message*/
    E_NIDD_MSG_METADATA      =  0x03,            /*!< Metadata message*/         
    E_NIDD_MSG_APPACT        =  0x20,            /*!< App activation message(D2C)*/
    E_NIDD_MSG_APPACTRESP    =  0x40,            /*!< App activation message (C2D)*/      
    E_NIDD_MSG_APPDEACT      =  0x21,            /*!< App deactivation message (D2C)*/         
    E_NIDD_MSG_APPDEACTRESP  =  0x41,            /*!< Acknowledgement packet (C2D)*/         
    E_NIDD_MSG_MAX
}jiot_nidd_msg_type_e;


/**
 * Enum represents content field in NIDD header
 */
typedef enum jiot_nidd_cntnt_type {
    E_NIDD_CNTNT_TEXT    =  0x00,            /*!< Plain Text format */
    E_NIDD_CNTNT_JSON    =  0x01,            /*!< Json Format */
    E_NIDD_CNTNT_BINARY  =  0x02,            /*!< Binary format */
    E_NIDD_CNTNT_MAX
}jiot_nidd_cntnt_type_e;


/**
 * Enum represents compression field in NIDD header
 */
typedef enum jiot_nidd_cmpr_type {
    E_NIDD_CMPR_PLD_UC           =  0x01,     /*!< Payload Uncompressed */
    E_NIDD_CMPR_TPC_UC           =  0x02,     /*!< Topic Uncompressed */       
    E_NIDD_CMPR_TPC_UC_PLD_UC    =  0x03,     /*!< Topic Uncompressed , Payload Uncompressed*/
    E_NIDD_CMPR_PLD_CP           =  0x11,     /*!< Payload Compressed */
    E_NIDD_CMPR_TPC_CP           =  0x22,     /*!< Topic Compressed */       
    E_NIDD_CMPR_TPC_UC_PLD_CP    =  0x13,     /*!< Topic Uncompressed , Payload Compressed*/
    E_NIDD_CMPR_TPC_CP_PLD_UC    =  0x23,     /*!< Topic Compressed , Payload Uncompressed*/
    E_NIDD_CMPR_TPC_CP_PLD_CP    =  0x33,     /*!< Topic Compressed , Payload Compressed*/
    E_NIDD_CMPR_MAX
}jiot_nidd_cmpr_type_e;


/**
* Enum repersent data delivery mode.   
*/
typedef enum jiot_nidd_delivery_mode {
    E_BEST_EFFORT_DELIVERY_MODE   =   0,   /*!< to represent best effort delivery mode*/
    E_GUARANTEED_DELIVERY_MODE    =   1    /*!< to represent guaranteed delivery mode*/
}jiot_nidd_delivery_mode_e;

/** 
 * This enumeration defines non-IP event which is reported from NIDD task.
 */
typedef enum jiot_nidd_cb_events {
    E_NIDD_SEND_SUCCESS    = 10, /**< Non-IP D2C data send Success.*/
    E_NIDD_SEND_FAILED     = 11, /**< Non-IP D2C data send Failed.*/
    E_NIDD_DATA            = 12, /**< Non-IP data (ack or C2D message) received.*/
    E_NIDD_MAX                    
} jiot_nidd_cb_events_e;

/* =======================================ENUM END================================================== */

/* ===================================STRUCTURE START================================================== */

/**
* jiot_nidd_target_info_t represent an event endpoint to/from where application can send/recv data
*/
typedef struct jiot_nidd_target_info {
    char* event; 
    char *tid; /**<transaction id . Applicable only for acknowledging the C2D messages */
    char *ahr; /**< application header. An Optional Field*/
    int evc;   /**< application event code */
    bool datacompressed;
}jiot_nidd_target_info_t;

/**
 * Structure represent data send/received.
 */
typedef struct jiot_nidd_data {
    uint8_t *p_data;
	uint16_t data_len;
}jiot_nidd_data_t;

/**
 * Structure represents header information of NIDD packets
 */
typedef struct __attribute__((__packed__)) {
	uint8_t protocol_discriminator;
	uint8_t version;
	uint8_t pkt_type;
	uint8_t msg_type;
	uint8_t content_type;
	uint8_t compr_type;
    uint32_t reserved;
}jiot_nidd_cmn_header_t;

/**
 * Structure represent payload meta data.
 */
typedef struct jiot_nidd_meta_data
{
    char* pld;         /*!< Platform ID */
    char* aid;         /*!< Application ID */
    char* svc;         /*!<Service code */
    char* eid;         /*!<Enterprise or User ID */
    char* vid;         /*!<Vendor ID */
}jiot_nidd_meta_data_t;

/* =====================================STRUCTURE END================================================== */

/* =====================================CALLBACK FUNCTION START=======================================*/

/*-----------------------------------------------------------------------------------------------*/
/**
* @brief  This is the context/handle of NIDD Middleware implementation.
*/
typedef void* jiot_nidd_handle_t;

/*-----------------------------------------------------------------------------------------------*/
/**
* @brief           The application registers a callback function to nidd using #jiot_nidd_registration()
*                  to register the non-ip event
*   @param[in] event The triggerred event type by NIDD task
*   @param[in] data   A pointer to the event data
*   @param[out] data_len - Actual data length
*   @return          None
*/
typedef void (jiot_nidd_callback)(jiot_nidd_cb_events_e event, void *data, uint16_t data_len);

/* =====================================CALLBACK FUNCTION END ======================================== */

/* =====================================FUNCTION START================================================== */

/*-----------------------------------------------------------------------------------------------*/
/**
* @brief This function registers the application by contacting JNOPS server and stores the app details. 
*
*   @param[in] appName - Name of the app whose details are to be fetched from JNOPS
*   @param[in] context - Context of middleware. Memory is allocated by callee and freed by callee.
*   @param[in] cb_options -  application callback .
*   @return - jiot_nidd_error_code_e 
*/
jiot_nidd_error_code_e jiot_nidd_registration(char *appName, jiot_nidd_handle_t *context, jiot_nidd_callback* cb_options);

/*-----------------------------------------------------------------------------------------------*/
/**
* @brief  This function will get the payload meta data, this data is used to form the entire payload
*         structure Memory is allocated by callee and freed by callee.
*   
*   @param[in] context - Context of middleware.
*   @return on success return jiot_nidd_meta_data_t otherwise NULL
*
*/
jiot_nidd_meta_data_t*  jiot_nidd_get_payload_meta_data(jiot_nidd_handle_t context);

/*-----------------------------------------------------------------------------------------------*/
/**
*  @brief This handler is used to send data over NIDD bearer
*
*   @param[in] context - Context of middleware.
*   @param[in] targetInfo -  ptr of jiot_nidd_target_info_t. (Memory of struct and it's element is allocate and free by caller).< Application can append transaction id to targetInfo > < Applicable only for acknowledging the C2D messages >
*   @param[in] appData - ptr of jiot_nidd_data_t. (Memory of struct and it's element is allocate and free by caller).
*   @param[in] deliveryMode - Mode in which the data has to be sent to server
*   @param[out] msgId - ID to identify the message sent
*   @return jiot_nidd_error_code_e
*
*/
jiot_nidd_error_code_e jiot_nidd_send(jiot_nidd_handle_t context, jiot_nidd_target_info_t* targetInfo, const jiot_nidd_data_t *appData,
    jiot_nidd_delivery_mode_e deliveryMode, int *msgId);

/*-----------------------------------------------------------------------------------------------*/
/**
*  @brief This is used by application to send out its data to the server, NaaS will not add any meta data header in payload.
*
*   @param[in] context - Context of middleware.
*   @param[in] targetInfo -  ptr of jiot_nidd_target_info_t. (Memory of struct and it's element is allocate and free by caller).< Application can append transaction id to targetInfo > < Applicable only for acknowledging the C2D messages >
*   @param[in] appData - ptr of jiot_nidd_data_t. (Memory of struct and it's element is allocate and free by caller).
*   @param[in] deliveryMode - Mode in which the data has to be sent to server
*   @param[in] contentType - content type of the appData
*   @param[out] msgId - ID to identify the message sent
*   @return jiot_nidd_error_code_e
*
*/
jiot_nidd_error_code_e jiot_nidd_app_send(jiot_nidd_handle_t context, jiot_nidd_target_info_t* targetInfo, const jiot_nidd_data_t *appData,
    jiot_nidd_delivery_mode_e deliveryMode,jiot_nidd_cntnt_type_e contentType, int *msgId);

/*-----------------------------------------------------------------------------------------------*/
/**
* @brief This function will update the device meta data by contacting JNOPS server.
*
*   @param[in] context - Context of middleware. Memory is allocated by callee and freed by callee.
*   Callback is expected via the function registered during jiot_nidd_registration() 
*   @return - jiot_nidd_error_code_e 
*/
jiot_nidd_error_code_e jiot_nidd_update_device_meta_data(jiot_nidd_handle_t context);

/** Set Config API */
/* As many as Set APIs can be defined as needed based on update meta data requirement */

/*-----------------------------------------------------------------------------------------------*/
/**
*  @brief This function used to set C2D channel
*   @param[in] c2dChannel name of the c2d channel whose value to be updated in config file
*   Example- mqtt,nidd
*   @return  none
*/
void jtos_conf_gen_set_c2dChannel(char* c2dChannel);

/*-----------------------------------------------------------------------------------------------*/
/**
*  @brief This function used to set protocol
*   @param[in] protocol name of the protocol whose value to be updated in config file
*   Example- mqtt,coap
*   @return  none
*/
void jiot_nidd_conf_gen_set_protocol(char* protocol);

/*-----------------------------------------------------------------------------------------------*/
/**
*  @brief This function used to set device model
*   @param[in]  DeviceModel name of the device model whose value to be updated in config file
*   @return  none
*/
void jiot_nidd_conf_gen_set_device_model(char* DeviceModel);

/*-----------------------------------------------------------------------------------------------*/
/**
* @brief  This handler destroys the NIDD session
*
*   @param[in] context - Context of middleware.
*   @return jiot_nidd_error_code_e
*
*/
jiot_nidd_error_code_e jiot_nidd_deregister(jiot_nidd_handle_t *context);

/* ===========================================FUNCTION END ======================================= */

#ifdef __cplusplus
}
#endif
#endif /* __JIOT_NIDD_API_H__ */

/* ===============================END OF THE FILE=============================================== */
