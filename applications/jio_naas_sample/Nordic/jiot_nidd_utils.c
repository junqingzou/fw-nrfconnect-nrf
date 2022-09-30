/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <nrf_modem_at.h>
#include "cJSON.h"
#include "cJSON_os.h"
#include "jiot_nidd_utils.h"

LOG_MODULE_REGISTER(utils, CONFIG_NAAS_LOG_LEVEL);

#define IMEI_SIZE 15
#define IMSI_SIZE 15

static struct k_sem nidd_sem;

/*-----------------------------------------------------------------------------------------------*/
/**
* @brief  This function dumped the hexadecimal data.
*
*   @param [in] buffer -Input hex buffer
*   @param [in] buffer_len - Actual Hex buffer size
*   @return None
*
*/
void jiot_nidd_dumphex(uint8_t *buffer, uint32_t buffer_len)
{
	LOG_HEXDUMP_INF(buffer, buffer_len, "nidd");
}

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
jiot_nidd_json_err_e jiot_nidd_json_parse_get_value(const char * json_buffer,const int length ,const char * keyname,char ** output, jiot_nidd_json_info_t* info)
{
	int ret;
	cJSON *json_objs;
	cJSON *json_data_obj;

	if ((json_buffer == NULL) || (keyname == NULL) || (info == NULL)) {
		LOG_ERR("JSON invalid param");
		return E_NIDD_JSON_FAILURE;
	}

	cJSON_Init();

	json_objs = cJSON_ParseWithLength(json_buffer, length);
	if (!json_objs) {
		LOG_ERR("JSON not found");
		return E_NIDD_JSON_FAILURE;
	}

	if (cJSON_HasObjectItem(json_objs, keyname)) {
		// json_data_obj = cJSON_GetObjectItemCaseSensitive(json_objs, keyname);
		json_data_obj = cJSON_GetObjectItem(json_objs, keyname);
		if (!json_data_obj) {
			LOG_ERR("JSON object get failed");
			ret = E_NIDD_JSON_FAILURE;
			goto cleanup;
		}
	} else {
		LOG_ERR("JSON object not found");
		ret = E_NIDD_JSON_FAILURE;
		goto cleanup;
	}
	if (cJSON_IsInvalid(json_data_obj)) {
		LOG_ERR("JSON object invalid");
		ret = E_NIDD_JSON_FAILURE;
		goto cleanup;

	}

	if (cJSON_IsNumber(json_data_obj) || cJSON_IsBool(json_data_obj) || cJSON_IsNull(json_data_obj)) {
		int *data = k_calloc(1, sizeof(int));
		if (data == NULL) {
			ret = E_NIDD_JSON_NOMEM;
			goto cleanup;
		}
		info->type = E_NIDD_JSON_PRIMITIVE;
		*data = json_data_obj->valueint;
		output = (char **)&data;
	} else if (cJSON_IsString(json_data_obj)) {
		char *data = k_calloc(strlen(json_data_obj->valuestring), sizeof(char));
		if (data == NULL) {
			ret = E_NIDD_JSON_NOMEM;
			goto cleanup;
		}
		strcpy(data, json_data_obj->valuestring);
		info->type = E_NIDD_JSON_STRING;
		output = &data;
	} else if (cJSON_IsArray(json_data_obj)) {
		LOG_WRN("JSON array type not supported");
		info->type = E_NIDD_JSON_ARRAY;
		ret = E_NIDD_JSON_FAILURE;
		goto cleanup;
	} else if (cJSON_IsObject(json_data_obj)) {
		LOG_WRN("JSON object type not supported");
		info->type = E_NIDD_JSON_OBJECT;
		ret = E_NIDD_JSON_FAILURE;
		goto cleanup;
	} else {
		LOG_WRN("JSON type not supported");
		ret = E_NIDD_JSON_FAILURE;
		goto cleanup;
	}

	ret = E_NIDD_JSON_SUCCESS;

cleanup:
	cJSON_Delete(json_objs);
	return ret;
}

/*-----------------------------------------------------------------------------------------------*/
/**
 * @brief	This API is abstraction of malloc function. Platform specific malloc implementation to be added .
 * 
 * @param[in] nbytes - Size of memory.
 * @return Pointer where the requested memory is allocated or NULL in case of failure	
 * 
 */
void* jiot_nidd_utility_malloc(size_t nbytes)
{
	return k_malloc(nbytes);
}

/*-----------------------------------------------------------------------------------------------*/
/**
 * @brief	This API is abstraction of calloc function. Platform specific calloc implementation to be added .
 * 
 * @param[in] nmemb - number of blocks to be allocated.
 * @param[in] size - Size of each block.
 * @return Pointer where the requested memory is allocated or NULL in case of failure	
 * 
 */
void* jiot_nidd_utility_calloc(size_t nmemb, size_t size)
{
	return k_calloc(nmemb, size);
}

/*-----------------------------------------------------------------------------------------------*/
/**
 * @brief	This API is abstraction of free function.Platform specific free implementation to be added .
 * 
 * @param[in] ptr - Pointer which is to be freed.
 * @return 	None
 * 
 */
void jiot_nidd_utility_free(void* ptr)
{
	if (ptr) {
		k_free(ptr);
	}
}

/*-----------------------------------------------------------------------------------------------*/
/**
 * @brief	This api return Device unique ID (i.e:"IMEI--IMSI").
 * 
 * @param[out] dev_uid - unique Id of Device. Memory is allocated by callee and freed by caller.
 * @return None
 * 
 */
void jiot_nidd_get_dev_uniqueId(char **dev_uid)
{
	int ret;
	char at_rsp[32];  /* 32 bytes is enough for below queries  */

	char *uid = k_calloc(IMEI_SIZE + 2 + IMSI_SIZE + 1, sizeof(char));
	if (uid == NULL) {
		LOG_ERR("No memeory");
		dev_uid = NULL;
		return;
	}
	ret = nrf_modem_at_cmd("AT+CGSN=1", sizeof(at_rsp), "%s", at_rsp);
	if (ret < 0) {
		LOG_ERR("Failed to read IMEI: %d", ret);
		k_free(uid);
		dev_uid = NULL;
		return;
	}
	strncpy(uid, at_rsp, IMEI_SIZE);
	strcat(uid, "--");

	ret = nrf_modem_at_cmd("AT+CIMI", sizeof(at_rsp), "%s", at_rsp);
	if (ret < 0) {
		LOG_ERR("Failed to read IMSI: %d", ret);
		k_free(uid);
		dev_uid = NULL;
		return;
	}
	strncpy(uid + IMEI_SIZE + 2, at_rsp, IMSI_SIZE);

	LOG_DBG("nidd_get_dev_uniqueId: %s", uid);
	dev_uid = &uid;
}

/*-----------------------------------------------------------------------------------------------*/
/**
* @brief	This api return current timestamp.
*
* @return current time stamp if sucess, Otherwise return NULL.  Memory is allocated by callee and freed by caller.
*
*/
char *jiot_nidd_get_curent_timeStamp()
{
	/*return system uptime in the format of Hours:Minutes:Seconds.Miniseconds */
	char *time_stamp = k_calloc(3 + 1 + 2 + 1 + 2 + 1 + 3 + 1, sizeof(char));
	if (time_stamp == NULL) {
		LOG_ERR("No memeory");
		return NULL;
	}

	uint64_t uptime = k_uptime_get();
	sprintf(time_stamp, "%03d:%02d:%02d.%03d",
		(int) uptime / (60 * 60 * 1000),
		(int)(uptime % (60 * 60 * 1000)) / (60 * 1000),
		(int)(uptime % (60 * 1000)) / 1000,
		(int) uptime % 1000);
	
	return time_stamp;
}

/*-----------------------------------------------------------------------------------------------*/
/**
*   jiot_nidd_osal_message_processor_create function will instantiate a JIOT message processor
*
*   @return jiot_nidd_osal_message_processor_create* - handle to the JIOT message processor
*
*/
jiot_nidd_osal_message_processor_t* jiot_nidd_osal_message_processor_create(jiot_nidd_osal_message_start_fn entry_fn)
{
	return NULL;
}

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
jiot_nidd_osal_err_e jiot_nidd_osal_message_processor_send(jiot_nidd_osal_message_processor_t* msgProcessor, jiot_nidd_osal_message_t* msg)
{
	return E_NIDD_OSAL_SUCCESS;
}

jiot_nidd_osal_message_t * jiot_nidd_osal_message_processor_readQueue(jiot_nidd_osal_message_processor_t*  msgProcessor)
{
	return NULL;
}

int jiot_nidd_osal_message_processor_isdone(jiot_nidd_osal_message_processor_t*  msgProcessor)
{
	return 0;
}

/*-----------------------------------------------------------------------------------------------*/
/**
*   This function will destroy the message processor.
*
*   @param[in,out] msgProcessor- address of handle to JIOT message processor
*	@return None
*
*/
void jiot_nidd_osal_message_processor_destroy(jiot_nidd_osal_message_processor_t** handle)
{
	
}

/*-----------------------------------------------------------------------------------------------*/
/**
*   This function will terminate the current thread
*
*   @return None
*/
void jiot_nidd_osal_message_processor_terminate(jiot_nidd_osal_message_processor_t*  msgProcessor)
{
	
}

/*-----------------------------------------------------------------------------------------------*/
/**
*   jiot_nidd_osal_semaphore_create function will instantiate a JIOT semaphore
*
*   @param[in] semValue - initial value of semaphore
*   @return handle to the JIOT semaphore on success otherwise NULL
*
*/
jiot_nidd_osal_semaphore_t* jiot_nidd_osal_semaphore_create(unsigned int semValue)
{
	if (k_sem_init(&nidd_sem, 0, semValue) != 0) {
		LOG_ERR("Failed to init semaphore");
		return NULL;
	}

	return (jiot_nidd_osal_semaphore_t *)&nidd_sem;
}

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
jiot_nidd_osal_err_e jiot_nidd_osal_semaphore_wait(jiot_nidd_osal_semaphore_t* semHandle,unsigned int millisecs) 
{
	int ret;
	struct k_sem *sem = (struct k_sem *)semHandle;

	if (millisecs > 0) {
		ret = k_sem_take(sem, K_MSEC(millisecs));
	} else {
		ret = k_sem_take(sem, K_FOREVER);
	}
	if (ret == 0) {
		return E_NIDD_OSAL_SUCCESS;
	} else if (ret == -EBUSY) {
		LOG_ERR("Semaphore busy");
		return E_NIDD_OSAL_FAILURE;   /* no no-waiting error defined */
	} else if (ret == -EAGAIN) {
		LOG_WRN("Semaphore wait timeout");
		return E_NIDD_OSAL_FAILURE;   /* no time-out error defined */
	} else {
		LOG_ERR("Semaphore wait failed: %d", ret);
		return E_NIDD_OSAL_FAILURE;
	}
}

/*-----------------------------------------------------------------------------------------------*/
/**
*   This function will increment or unlocks the semaphore. If the semaphore value is become greater than 0 then other
*   thread waiting on this semaphore will be woken up and the proceed to lock the semaphore. 
*
*   @param[in] semHandle- handle to JIOT semaphore
*   @return jiot_nidd_osal_err_e
*
*/
jiot_nidd_osal_err_e jiot_nidd_osal_semaphore_post(jiot_nidd_osal_semaphore_t* semHandle)
{
	struct k_sem *sem = (struct k_sem *)semHandle;
	k_sem_give(sem);

	return E_NIDD_OSAL_SUCCESS;
}

/*-----------------------------------------------------------------------------------------------*/
/**
*   This function will destroy the JIOT semaphore
*
*   @param[in,out] msgProcessor- address of handle to JIOT semaphore
*
*/
void jiot_nidd_osal_semaphore_destroy(jiot_nidd_osal_semaphore_t** semHandle)
{
	struct k_sem *sem = (struct k_sem *)semHandle;
	k_sem_reset(sem);
}

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
int jiot_nidd_utility_file_open(unsigned char *fileName, jiot_nidd_pal_file_flag_e mode, void **fd)
{
	return -128;
}

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
int32_t jiot_nidd_utility_file_write(void *fd, uint32_t offset, uint32_t noOfBytes, unsigned char *writeBuffer)
{
	return -128;
}

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
int32_t jiot_nidd_utility_file_read(void *fd, uint32_t offset, uint32_t noOfBytes, unsigned char *readBuffer)
{
	return -128;
}

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
int32_t jiot_nidd_utility_file_tell(void *fd)
{
	return -128;
}

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
int8_t jiot_nidd_utility_file_seek(void *fd, uint32_t offset, jiot_nidd_pal_file_whence_e whence)
{
	return -128;
}

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
int32_t jiot_nidd_utility_file_remove(unsigned char *fileName)
{
	return -128;
}

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
int8_t jiot_nidd_utility_file_close(void *fd)
{
	return -128;
}
