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
*
* =============================================================================================== */


/* ===============================FILE REVISION HISTORY============================================
  Date         Author(RIL EMail ID)      CLIM ID(if applicable)       Detail of changes
--------       --------------------      ----------------------       -----------------
31-05-2021            JPL                         NONE                        CREATED     
===============================ENDOF REVISION HISTORY============================================ */

/* ===============================ABOUT THIS FILE================================================ */

/**
* @file jiot_nidd_plat_abs.h
* @brief NIDD abstraction APIs, to be implemented by SoC-vendor/ODM. Jio NIDD server-interface protocol 
*        to be implemented on top of these APIs.
*/
/*=================================END ABOUT THIS FILE=========================================== */
#ifndef __JIOT_NIDD_PLAT_ABS_H__
#define __JIOT_NIDD_PLAT_ABS_H__

#if defined(__cplusplus)
extern "C"
{
#endif


/* ===============================INCLUDE START================================================= */
#include <stdint.h>
#include <stdbool.h>
/* ===============================INCLUDE END ================================================== */



/* ===============================#DEFINE START================================================= */


/* ===============================#DEFINE END=================================================== */



/* ===============================ENUM START==================================================== */


/*-----------------------------------------------------------------------------------------------*/
/**
 *  @brief This enumeration repersent non-IP event which is reported from NIDD thread.
 */
typedef enum jiot_nidd_plat_event{
    E_NIDD_PLAT_EVENT_CHANNEL_ACTIVATE_IND    = 0, /**< Event generated when Non-IP channel is activated.*/
    E_NIDD_PLAT_EVENT_CHANNEL_DEACTIVATE_IND  = 1, /**< Event generated when Non-IP channel is deactivated.*/
    E_NIDD_PLAT_EVENT_DATA_IND                = 2, /**< Event generated when Non-IP data is received.*/
    E_NIDD_PLAT_EVENT_MAX                     = 0xFF
} jiot_nidd_plat_event_e;

/*-----------------------------------------------------------------------------------------------*/
/**
 *  @brief This enumeration repersent non-IP error code. Below are some error codes 
 */
typedef enum jiot_plat_nidd_ret{
    E_NIDD_PLAT_RET_OK             =  0, /**< No error.*/
    E_NIDD_PLAT_RET_ERROR          = -1, /**< Common error.*/
    E_NIDD_PLAT_RET_INACTIVE_ERROR = -2, /**< PDN is not activated.*/
    E_NIDD_PLAT_RET_BUSY           = -3, /**< Operation is busy.*/
    E_NIDD_PLAT_RET_NO_MEMORY      = -4, /**< No memory.*/
    E_NIDD_PLAT_RET_WOULDBLOCK     = -5, /**< Operation would block.*/
    E_NIDD_PLAT_RET_END            = 0xFF
} jiot_plat_nidd_ret_e;
/*-----------------------------------------------------------------------------------------------*/

/* ===============================ENUM END====================================================== */



/* ===============================TYPEDEF START================================================= */

/*-----------------------------------------------------------------------------------------------*/
/**
 *  @brief This structure repersent non-IP data indication structure reported by JIOT_NIDD_PLAT_EVENT_DATA_IND.
 */
typedef struct jiot_nidd_plat_data_ind{
    uint32_t nidd_id;       /*The nidd id created by function: nidd_connect.*/
    uint32_t data_len;      /*Non-IP data length.*/
    uint8_t* p_data;        /*Non-IP data pointer.*/
} jiot_nidd_plat_data_ind_t;


/*-----------------------------------------------------------------------------------------------*/

/* ===============================TYPEDEF END=================================================== */


/* ===============================CALLBACK FUNCTION START ====================================== */

/*************************************************************************************************************
* @brief           The application registers a callback function to the nidd using #nidd_connect()
*                  to register non-ip event.
* @param[in] event The triggerred event type by NIDD thread.
* @param[in] pdata A pointer to the event data.
* @param[in] data_len Length of the data received . 
* @return          None.
**************************************************************************************************************/
typedef void (* jiot_nidd_plat_event_handler)(jiot_nidd_plat_event_e event, void *data , uint16_t data_len);

/*-----------------------------------------------------------------------------------------------*/

/* ===============================CALLBACK FUNCTION END ======================================== */


/* ====================================== FUNCTION START ======================================= */
	

/*************************************************************************************************************
* @brief           Create a nidd id for specific APN.
* @param[in/out]   nidd_id -- the generated nidd id.
* @param[in]       apn     -- APN name.
* @param[in]       callback -- register the event callback to NIDD task.
* @return          jiot_plat_nidd_ret_e.
**************************************************************************************************************/
jiot_plat_nidd_ret_e jiot_nidd_plat_connect(uint32_t* nidd_id, char* apn, jiot_nidd_plat_event_handler callback);


/*************************************************************************************************************
* @brief           Check whether the NIDD is activated.
* @param[in]       nidd_id -- the generated nidd id.
* @return          true: NIDD is activated, false: NIDD is not active.
**************************************************************************************************************/
bool jiot_nidd_plat_is_nidd_activated(uint32_t nidd_id);


/*************************************************************************************************************
* @brief           Send NIDD data to modem.
* @param[in]       nidd_id -- the 
* @param[in]       data    -- points to the data buffer..
* @param[in]       length  -- data length.
* @return          refer to jiot_plat_nidd_ret_e.
**************************************************************************************************************/
jiot_plat_nidd_ret_e jiot_nidd_plat_send_data(uint32_t nidd_id, void* data, uint16_t length);


/* ====================================== FUNCTION END ========================================= */
#ifdef __cplusplus
}
#endif

#endif /* __JIOT_NIDD_PLAT_ABS_H__ */
/* ===============================END OF THE FILE=============================================== */
