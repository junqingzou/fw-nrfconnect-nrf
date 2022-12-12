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
*============================================================================================= */


/* ===============================FILE REVISION HISTORY============================================
  Date         Author(RIL EMail ID)      CLIM ID(if applicable)       Detail of changes
--------       --------------------      ----------------------       -----------------
31-05-2021            JPL                         NONE                        CREATED

===============================ENDOF REVISION HISTORY============================================ */

/* ===============================ABOUT THIS FILE=============================================== */

/**
* @file jiot_nidd_utils.h
* @brief Contains prototype of various utility functions used in jiot_nidd_main.c. These functions to be implemented by SoC-vendor/ODM.
*/

/*  ===============================END ABOUT THIS FILE=========================================== */

#ifndef __JIOT_NIDD_UTILS_H__
#define __JIOT_NIDD_UTILS_H__

#ifdef __cplusplus
extern "C"{
#endif
/* ===============================INCLUDE START================================================= */

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/kernel.h>

/* ===============================INCLUDE END=================================================== */

/* ===============================DEFINE START================================================= */
  #define JIOT_NIDD_JSON_MAX_TOKEN 	128
#if 0
  #define JIOT_NIDD_LOG_E(...) jiot_nidd_log( E_NIDD_LOG_ERROR, __FILE__, __LINE__,##__VA_ARGS__ ) 
  #define JIOT_NIDD_LOG_W(...) jiot_nidd_log( E_NIDD_LOG_WARNING, __FILE__, __LINE__,##__VA_ARGS__ ) 
  #define JIOT_NIDD_LOG_I(...) jiot_nidd_log( E_NIDD_LOG_INFO, __FILE__, __LINE__,##__VA_ARGS__ ) 
  #define JIOT_NIDD_LOG_D(...) jiot_nidd_log( E_NIDD_LOG_DEBUG, __FILE__, __LINE__,##__VA_ARGS__ ) 
#else
  #define JIOT_NIDD_LOG_E	LOG_ERR
  #define JIOT_NIDD_LOG_W	LOG_WRN
  #define JIOT_NIDD_LOG_I	LOG_INF
  #define JIOT_NIDD_LOG_D(...)
#endif
  
/* ===============================DEFINE END================================================= */

/* ===============================ENUM START================================================== */
/**
 * Enum repersent the json parsing error code.
 */
typedef enum jiot_nidd_json_err {
	E_NIDD_JSON_SUCCESS   =  0,  /*!<When parsing was successful*/
	E_NIDD_JSON_NOMEM     = -1,  /*!<Max tokens allowed is JSON_MAX_TOKEN, if the JSON is having tokens higher than this , E_NIDD_JSU_JSON_NOMEM would be returned*/
	E_NIDD_JSON_FAILURE	  = -2   /*!<When parsing failed due to invalid character inside Json string or if the passed JSON is incomplete*/
} jiot_nidd_json_err_e;

/**
 *  Enum repersent JSON Type which would be parsed 
 */
typedef enum jiot_nidd_json_type {
	E_NIDD_JSON_UNDEFINED   = 0,    
	E_NIDD_JSON_OBJECT      = 1,   	/*!<JSON type : Object*/
	E_NIDD_JSON_ARRAY       = 2,    /*!<JSON type : Array*/
	E_NIDD_JSON_STRING      = 3,   	/*!<JSON type : String*/
	E_NIDD_JSON_PRIMITIVE   = 4 	/*!<JSON type : primitive (number, boolean (true/false) or null)*/
} jiot_nidd_json_type_e;


 /**  
  * Enum repersent NIDD OSAL error codes
 */
typedef enum jiot_nidd_osal_err {
	E_NIDD_OSAL_SUCCESS   	 = 0,    /*!<NIDD OSAL Operation Success*/
	E_NIDD_OSAL_FAILURE      = 1,    /*!<NIDD OSAL Operation Failed*/
	E_NIDD_OSAL_NO_MEM       = 2,    /*!<NIDD OSAL Out of memory*/
	E_NIDD_OSAL_MAX
} jiot_nidd_osal_err_e;

/**
* Enum repersent Logging Level 
*/
typedef enum jiot_nidd_log_level {
	E_NIDD_LOG_ERROR      = 0, 		/*!< Error. incluedes Error level*/
	E_NIDD_LOG_WARNING    = 1, 		/*!< Warning includes warnings level*/
	E_NIDD_LOG_INFO       = 2,		/*!< Information. includes info. level*/
	E_NIDD_LOG_DEBUG      = 3,		/*!< Debug. includes debug Level.*/
}jiot_nidd_log_level_e;

/**
* This enum indicate the file open operation status
*/
typedef enum
{
	E_NIDD_PAL_FILE_FLAG_WRITE,		/*!< Open the file in write mode. Created file if it does not exist. */
	E_NIDD_PAL_FILE_FLAG_READ,		/*!< Open the file in read. */
	E_NIDD_PAL_FILE_FLAG_APPEND,		/*!< Open the file in append mode. Created file if it does not exist. */
	E_NIDD_PAL_FILE_FLAG_WRITEPLUS,		/*!< Open the file in read and write mode. Created file if it does not exist. */
	E_NIDD_PAL_FILE_FLAG_READPLUS,		/*!< Open the file in read and write mode. */
	E_NIDD_PAL_FILE_FLAG_APPENDPLUS,		/*!< Open the file in read and append mode. Created file if it does not exist. */
}jiot_nidd_pal_file_flag_e;

/**
* This enum indicate the whence in which seek operation need to perform*/
typedef enum
{
	E_NIDD_PAL_FILE_WHENCE_START,		/*!< It denotes starting of the file. */
	E_NIDD_PAL_FILE_WHENCE_END,		/*!< It denotes end of the file. */
	E_NIDD_PAL_FILE_WHENCE_CUR,		/*!< It denotes file pointer's current position. */
}jiot_nidd_pal_file_whence_e;


/**
* This enum indicate config file operation status*/
typedef enum
{
   E_NIDD_CONFIG_SUCCESS = 0,
   E_NIDD_CONFIG_FAILURE = -1
}jiot_nidd_config_file_err_e;

/* ===============================ENUM END================================================== */

/* ===============================TYPEDEF START================================================== */
/*-----------------------------------------------------------------------------------------------*/
/**
*   jiot_nidd_osal_message_start_fn represent a start function for the thread to be created
*
*/
typedef void(*jiot_nidd_osal_message_start_fn)(void);

/*-----------------------------------------------------------------------------------------------*/
/**
*   jiot_nidd_osal_message_handler represent a handler to process application message
*
*   This handler is an application handler to process application message in a separate thread context 
*    which is own by the message processor
*/
typedef void(*jiot_nidd_osal_message_handler)(void*);

/*-----------------------------------------------------------------------------------------------*/
/**
*   jiot_nidd_osal_message_processor_t represent a message processor 
*
*   This is an opaque type. Application is allowed to only define the pointers of this type. Underlying platform 
*   has to provide the impementation of this type as it requires platform specific resources.This message processor
*   is basically a thread and message queue where thread get the message form the queue and start processing in its context.
*	eg[FreeRtos Os] 
*	typedef struct jiot_nidd_osal_message_processor
*	{
*	TaskHandle_t worker;
*	QueueHandle_t queue;
*	EventGroupHandle_t event;
*	}jiot_nidd_osal_message_processor_t;
*/
typedef struct k_msgq jiot_nidd_osal_message_processor_t;

/*-----------------------------------------------------------------------------------------------*/
/**
*   jiot_nidd_osal_semaphore_t represent JIOT semaphore 
*
*   This is an opaque type, so application is allowed to define the pointer of this type and the underlying
*   semaphore object can be managed using the JIOT semaphore life cycle API. This opaque type can be implemented by 
*   different OS using their underlying capabilities.
* eg[FreeRtos Os]: 
*		typedef struct jiot_nidd_osal_semaphore
*		{
*   		SemaphoreHandle_t *sem;
*		}jiot_nidd_osal_semaphore_t;
*/
typedef struct k_sem jiot_nidd_osal_semaphore_t;

/* ===============================TYPEDEF END================================================== */

/* ===============================STRUCTURE START================================================== */

/**
 *  Structure repersent JSON information after parsing
 */
typedef struct jiot_nidd_json_info {
	jiot_nidd_json_type_e type; 		/*!<JSON type : (object, array, string etc.)*/
	uint16_t start;                  	/*!<start : indicates the start position of the JSON data string.*/
	uint16_t end;                    	/*!<end	: indicates the end position of the JSON data string.*/
} jiot_nidd_json_info_t;

/**
*   jiot_nidd_osal_message_t represent a message processor message 
*	This structure is composition of application handler and application message
*/
typedef struct jiot_nidd_osal_message {
    jiot_nidd_osal_message_handler msgHandler;
    void* message;
}jiot_nidd_osal_message_t;

/* ===============================STRUCTURE END================================================== */

/* ===============================FUNCTION START================================================== */

/*-----------------------------------------------------------------------------------------------*/
/**
* @brief  This function dumped the hexadecimal data.
*
*   @param [in] buffer -Input hex buffer
*   @param [in] buffer_len - Actual Hex buffer size
*   @return None
*
*/
void jiot_nidd_dumphex(uint8_t *buffer, uint32_t buffer_len);

/*-----------------------------------------------------------------------------------------------*/
/**
*   This API parses the JSON string and searches for the keyname in the string. If successfull, the output will be filled with a valid pointer which has the value of the key requested.
*
*   @param [in] json_buffer - Json buffer to be parsed
*   @param [in] length - Length of the Json buffer to be parsed
*   @param [in] keyname - Keyname (string) whose value needs to be searched in json_buffer
*   @param [out] outData - outdata (string) will hold value if parsing is successful. Memory for ‘output’ will be allocated inside this API and caller will have to free the same
*   @param [out] info - information of the keyvalue  
*   @return E_NIDD_JSU_JSON_SUCCESS on success,  E_NIDD_JSU_JSON_NOMEM when JSON buffer with more than JSON_MAX_TOKEN are passed 
*           otherwise E_NIDD_JSON_FAILURE
*
*/
jiot_nidd_json_err_e jiot_nidd_json_parse_get_value(const char * json_buffer,const int length ,const char * keyname,char ** output, jiot_nidd_json_info_t* info);

/*-----------------------------------------------------------------------------------------------*/
/**
 * @brief	This API is abstraction of malloc function. Platform specific malloc implementation to be added .
 * 
 * @param[in] nbytes - Size of memory.
 * @return Pointer where the requested memory is allocated or NULL in case of failure	
 * 
 */
void* jiot_nidd_utility_malloc(size_t nbytes);

/*-----------------------------------------------------------------------------------------------*/
/**
 * @brief	This API is abstraction of calloc function. Platform specific calloc implementation to be added .
 * 
 * @param[in] nmemb - number of blocks to be allocated.
 * @param[in] size - Size of each block.
 * @return Pointer where the requested memory is allocated or NULL in case of failure	
 * 
 */
void* jiot_nidd_utility_calloc(size_t nmemb, size_t size);

/*-----------------------------------------------------------------------------------------------*/
/**
 * @brief	This API is abstraction of free function.Platform specific free implementation to be added .
 * 
 * @param[in] ptr - Pointer which is to be freed.
 * @return 	None
 * 
 */
void jiot_nidd_utility_free(void* ptr);

/*-----------------------------------------------------------------------------------------------*/
/**
 * @brief	This api return Device unique ID (i.e:"IMEI--IMSI").
 * 
 * @param[out] dev_uid - unique Id of Device. Memory is allocated by callee and freed by caller.
 * @return None
 * 
 */
void  jiot_nidd_get_dev_uniqueId(char **dev_uid);

/*-----------------------------------------------------------------------------------------------*/
/**
* @brief	This api return current timestamp.
*
* @return current time stamp if sucess, Otherwise return NULL.  Memory is allocated by callee and freed by caller.
*
*/
char *jiot_nidd_get_curent_timeStamp();

/*-----------------------------------------------------------------------------------------------*/
/**
*   jiot_nidd_osal_message_processor_create function will instantiate a JIOT message processor
*
*   @return jiot_nidd_osal_message_processor_create* - handle to the JIOT message processor
*
*/
jiot_nidd_osal_message_processor_t* jiot_nidd_osal_message_processor_create(jiot_nidd_osal_message_start_fn entry_fn);

/*-----------------------------------------------------------------------------------------------*/
/**
*   This function will send the application message to the message processor which will execute
*   in the message processor thread context. This function will post the message into the 
*   message processor queue and return.
*
*   @param[in] msgProcessor- handle to JIOT message processor
*   @param[in] msg - msg which need to be sent to the message processor for execution
*   @return jiot_nidd_osal_err_e
*
*/
jiot_nidd_osal_err_e jiot_nidd_osal_message_processor_send(jiot_nidd_osal_message_processor_t* msgProcessor, jiot_nidd_osal_message_t* msg);

/*-----------------------------------------------------------------------------------------------*/
/**
*   This function will destroy the message processor.
*
*   @param[in,out] msgProcessor- address of handle to JIOT message processor
*	@return None
*
*/
void jiot_nidd_osal_message_processor_destroy(jiot_nidd_osal_message_processor_t** handle);

/*-----------------------------------------------------------------------------------------------*/
/**
*   This function will terminate the current thread
*
*   @return None
*/
void jiot_nidd_osal_message_processor_terminate(jiot_nidd_osal_message_processor_t*  msgProcessor);

/*-----------------------------------------------------------------------------------------------*/
/**
*   jiot_nidd_osal_semaphore_create function will instantiate a JIOT semaphore
*
*   @param[in] semValue - initial value of semaphore
*   @return handle to the JIOT semaphore on success otherwise NULL
*
*/
jiot_nidd_osal_semaphore_t* jiot_nidd_osal_semaphore_create(unsigned int semValue);

/*-----------------------------------------------------------------------------------------------*/
/**
*   This function will decrement or locks the semaphore. If the semaphore value is greater than 0 then decremnts
*   proceed and the function return immediately value by 1. If the current value of semaphore is 0 then the call blocks
*   till the block period for either its value becomes greater than 0 or a signal handler interrupts the call.
*
*   @param[in] semHandle- handle to JIOT semaphore             
*   @param[in] millisecs- Time to wait for semaphore. 0 - wait indefinitely else wait for specified time in millisecs                                  
*   @return jiot_nidd_osal_err_e
*
*/
jiot_nidd_osal_err_e jiot_nidd_osal_semaphore_wait(jiot_nidd_osal_semaphore_t* semHandle,unsigned int millisecs);

/*-----------------------------------------------------------------------------------------------*/
/**
*   This function will increment or unlocks the semaphore. If the semaphore value is become greater than 0 then other
*   thread waiting on this semaphore will be woken up and the proceed to lock the semaphore. 
*
*   @param[in] semHandle- handle to JIOT semaphore
*   @return jiot_nidd_osal_err_e
*
*/
jiot_nidd_osal_err_e jiot_nidd_osal_semaphore_post(jiot_nidd_osal_semaphore_t* semHandle);

/*-----------------------------------------------------------------------------------------------*/
/**
*   This function will destroy the JIOT semaphore
*
*   @param[in,out] msgProcessor- address of handle to JIOT semaphore
*
*/
void jiot_nidd_osal_semaphore_destroy(jiot_nidd_osal_semaphore_t** semHandle);


jiot_nidd_osal_message_t * jiot_nidd_osal_message_processor_readQueue(jiot_nidd_osal_message_processor_t*  msgProcessor);

int jiot_nidd_osal_message_processor_isdone(jiot_nidd_osal_message_processor_t*  msgProcessor);

/*-----------------------------------------------------------------------------------------------*/
/**
 * @brief  This function is used to open the file.
 * @param[in] fileName - number of blocks to be allocated.
 * @param[in] mode - Size of each block.
 * @param[Out] fd - Need to store the void pointer which indicate file discriptor.
 * @return, 0 on success else,
 *		-1 if invalid file name,
 *		-2 if invalid mode,
 *		-3 if fail to open,
 *		-128 if API not supported. *
 */
int jiot_nidd_utility_file_open(unsigned char *fileName, jiot_nidd_pal_file_flag_e mode, void **fd);
/*-----------------------------------------------------------------------------------------------*/
/**
 * @brief  This function writes the content in to the file.
 *
 * @param[in] fd - This parameter specifies the file discriptor to which data to write.
 * @param[in] offset - This parameter specified the offset to which the data has to be written.
 * @param[in] noOfBytes - This parameter specifies the number of bytes to be written from the specific offset.
 * @param[in] readBuffer - This parameter is an in parameter buffer from which the data will be written.
 * @return ,total number of bytes to be writen else,
 *		                -1 if invalid file discriptor,
 *				-2 if invalid number of bytes to be write(noOfBytes),
 *				-3 if invalid write buffer,
 *				-4 if file details corrupt,
 *				-5 if file size less then number of bytes to be write,
 *				-6 if fail to open file,
 *				-7 if fail to jump on specific offset,
 *				-8 if fail to write,
 *				-128 if API not supported.
 */
int32_t jiot_nidd_utility_file_write(void *fd, uint32_t offset, uint32_t noOfBytes, unsigned char *writeBuffer);
/*-----------------------------------------------------------------------------------------------*/
/**
 * @brief  This function will read a particular offset and no of bytes from the specified file.
 *
 * @param[in] fd - This parameter specifies the file discriptor from which data to read.
 * @param[in] offset - This parameter specified the offset from which the data has to be read.
 * @param[in] noOfBytes - This parameter specifies the number of bytes to be read from the specific offset.
 * @param[out] readBuffer - buffer in which the stored data will be read.
 * @return total number of bytes which is to be read else,
 *				-1 if invalid file discriptor,
 *				-2 if invalid number of bytes to be read(noOfBytes),
 *				-3 if invalid read buffer,
 *				-4 if file details corrupt,
 *				-5 if file size less then number of bytes to be read,
 *				-6 if fail to open file,
 *				-7 if fail to jump on specific offset,
 *				-8 if fail to read,
 *				-128 if API not supported.
 *
 */
int32_t jiot_nidd_utility_file_read(void *fd, uint32_t offset, uint32_t noOfBytes, unsigned char *readBuffer);
/*-----------------------------------------------------------------------------------------------*/
/**
 * @brief  Function obtains the current value of the file position indicator.
 *
 * @param[in] fd -This parameter specifies the file discriptor.
 * @return ,the current offset,
 *	-1 if invalid file discriptor,
 *	-2 if tell fails,
 *	-128 if API not supported.
 */
 int32_t jiot_nidd_utility_file_tell(void *fd);
/*-----------------------------------------------------------------------------------------------*/
/**
 * @brief This function is used for reposition a stream.
 *
 * @param[in] fd - This parameter specifies the file discriptor to which data is to be seek.
 * @param[in] offset - Indicate the offset by which fd has to be moved.
 * @param[in] whence - Indicate the mode mentioned in jiot_nidd_pal_file_whence_e for reposition a file discriptor.
 *
 * @return ,0 on success,
 *	    -1 if invalid file discriptor,
 *	    -2 if fail to jump on specific offset,
 *	    -3 if seek fails,
 *	    -128 if API not supported.
 *
 */
int8_t jiot_nidd_utility_file_seek(void *fd, uint32_t offset, jiot_nidd_pal_file_whence_e whence);
/*-----------------------------------------------------------------------------------------------*/
/**
 * @brief  This function is used to delete the file.
 *
 * @param[in] fileName - Name of the file which is to be deleted.
 * @return,0 on success else,
 *
 *		-1 if invalid file name,
 *		-2 if file not present,
 *		-3 if fail to remove,
 *		-128 if API not supported.
 *
 */
int32_t jiot_nidd_utility_file_remove(unsigned char *fileName);
/*-----------------------------------------------------------------------------------------------*/
/**
 * @brief  This function is used to close the file.
 *
 * @param[in] fd -This parameter specifies the file discriptor.
 * @return 0 on success,
 *	   -1 if invalid file discriptor,
 *	   -2 if close fail,
 *	   -128 if API not supported.
 */
int8_t jiot_nidd_utility_file_close(void *fd);
/* ===============================FUNCTION END================================================== */
#ifdef __cplusplus
}
#endif
#endif /* __JIOT_NIDD_UTILS_H__ */
/* ===============================END OF FILE ================================================== */
