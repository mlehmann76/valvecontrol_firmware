/*
 * config.c
 *
 *  Created on: 28.04.2019
 *      Author: marco
 */

#include <string.h>
#include <stdlib.h>

#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "cJSON.h"
const char *TAG = "config";
static nvs_handle my_handle;
char *nvs_json_config = NULL;

extern const char config_json_start[] asm("_binary_config_json_start");
//extern const size_t config_json_size asm("_binary_config_json_size");
//extern const char* config_json_end   asm("_binary_config_json_end");

static esp_err_t readStr(nvs_handle *pHandle, const char *pName, char **dest);
static esp_err_t writeStr(nvs_handle *pHandle, const char *pName, const char *str);

/**/
static esp_err_t readStr(nvs_handle *pHandle, const char *pName, char **dest) {
	size_t required_size;
	char * temp;
	esp_err_t err = nvs_get_str(*pHandle, pName, NULL, &required_size);
	if (err == ESP_OK) {
		temp = malloc(required_size);
		if (temp != NULL) {
			nvs_get_str(*pHandle, pName, temp, &required_size);
			*dest = temp;
		} else {
			err = ESP_ERR_NO_MEM;
		}
	}
	return err;
}

static esp_err_t writeStr(nvs_handle *pHandle, const char *pName, const char *str) {
	return nvs_set_str(*pHandle, pName, str);
}
/**/

esp_err_t configInit(void) {
	esp_err_t ret = ESP_OK;

	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		// NVS partition was truncated and needs to be erased
		// Retry nvs_flash_init
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}

	ESP_ERROR_CHECK(err);

	err = nvs_open("storage", NVS_READWRITE, &my_handle);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "nvs_open storage failed (%s)", esp_err_to_name(err));
	}

	//try opening saved json config
	if (ESP_OK == readStr(&my_handle, "config_json", &nvs_json_config)) {
		//required_size = strlen(nvs_json_config);
	} else {
		//use default config
		nvs_json_config = (char*)config_json_start;
	}

	//
	return ret;
}

static char* readJsonConfigStr(const cJSON *pRoot, const char* section, const char* name) {
	char* ret = NULL;
	cJSON* pSection = cJSON_GetObjectItem(pRoot, section);
	if (pSection != NULL) {
		cJSON* pName = cJSON_GetObjectItem(pSection, name);

		if (cJSON_IsString(pName) && (pName->valuestring != NULL)) {
			ret = malloc(strlen(pName->valuestring));
			strcpy(ret, pName->valuestring);
			ESP_LOGI(TAG, "section (%s), name (%s) in default found (%s)", section, name, ret);
		} else {
			ESP_LOGE(TAG, "name in default not found (%s)", name);
		}
	} else {
		ESP_LOGE(TAG, "section in default not found (%s)", section);
	}
	return ret;
}

esp_err_t readConfigStr(const char* section, const char* name, char **dest) {
	char* ret = NULL;

	if (nvs_json_config != NULL) {
		cJSON *root = cJSON_Parse(nvs_json_config);
		ret = readJsonConfigStr(root, section, name);
		cJSON_Delete(root);
	}

	if (ret == NULL) {
		cJSON *root = cJSON_Parse(config_json_start);
		ret = readJsonConfigStr(root, section, name);
		cJSON_Delete(root);
	}

	*dest = ret;
	return ret != NULL ? ESP_OK : !ESP_OK;
}
