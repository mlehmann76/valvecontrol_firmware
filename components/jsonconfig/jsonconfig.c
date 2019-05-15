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

static esp_err_t readStr(nvs_handle *pHandle, const char *pName, char **dest);
static esp_err_t writeStr(nvs_handle *pHandle, const char *pName, const char *str);

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
	err = readStr(&my_handle, "config_json", &nvs_json_config);
	if (ESP_OK != err) {
		ESP_LOGE(TAG, "config_json read failed (%s)", esp_err_to_name(err));
	}
	//
	return ret;
}

void debugJson(const char* TAG, cJSON* root) {
	/* */
	char* out = NULL;
	out = cJSON_Print(root);
	ESP_LOGV(TAG, "new config \n%s", out);
	free(out);
}

/*
 *
 */
void updateConfig(const char *pjsonConfig) {
	cJSON *root = NULL, *pUpdate = NULL, *pUpdateItem = NULL;

	// read recent config
	if (nvs_json_config != NULL) {
		root = cJSON_Parse(nvs_json_config);
	}

	if (root == NULL) {
		root = cJSON_Parse(config_json_start);
	}

	pUpdate = cJSON_Parse(pjsonConfig);
	if (root != NULL) {
		if (pUpdate != NULL) {
			// check for updated items
			cJSON_ArrayForEach(pUpdateItem, pUpdate)
			{
				// replace item
				if (pUpdateItem->string != NULL) {
					cJSON *dup = cJSON_Duplicate(pUpdateItem, true);
					ESP_LOGI(TAG, "testing (%s)", dup->string);
					if (cJSON_HasObjectItem(root, dup->string)) {
						//string gets lost in ReplaceItemInObject
						char s[128];
						strncpy(s, dup->string, sizeof(s));
						debugJson(TAG, dup);
						ESP_LOGI(TAG, "replacing (%s)", s);
						cJSON_ReplaceItemInObject(root, s, dup);
						debugJson(TAG, root);
					}
				}
			}
		} else {
			ESP_LOGE(TAG, "error parsing update (%s)", pjsonConfig);
		}

	} else {
		ESP_LOGE(TAG, "error parsing nvs and default");
	}

	// write config
	char* out = NULL;
	out = cJSON_Print(root);
	ESP_LOGI(TAG, "writing \n%s", out);
	esp_err_t err = writeStr(&my_handle, "config_json", out);
	free(out);

	if (ESP_OK != err) {
		ESP_LOGE(TAG, "config_json write failed (%s)", esp_err_to_name(err));
	}

	//clean up
	if (root != NULL) {
		cJSON_Delete(root);
	}

	if(pUpdate != NULL) {
		cJSON_Delete(pUpdate);
	}
}

static char* readJsonConfigStr(const cJSON *pRoot, const char *cfg, const char* section, const char* name) {
	char* ret = NULL;
	cJSON* pSection = cJSON_GetObjectItem(pRoot, section);
	if (pSection != NULL) {
		cJSON* pName = cJSON_GetObjectItem(pSection, name);

		if (cJSON_IsString(pName) && (pName->valuestring != NULL)) {
			ret = malloc(strlen(pName->valuestring));
			strcpy(ret, pName->valuestring);
			ESP_LOGI(TAG, "section (%s), name (%s) in (%s) found (%s)", section, name, cfg, ret);
		} else {
			ESP_LOGE(TAG, "name in (%s) not found (%s)", cfg, name);
		}
	} else {
		ESP_LOGE(TAG, "section in (%s) not found (%s)", cfg, section);
	}
	return ret;
}

esp_err_t readConfigStr(const char* section, const char* name, char **dest) {
	char* ret = NULL;

	if (nvs_json_config != NULL) {
		cJSON *root = cJSON_Parse(nvs_json_config);
		ret = readJsonConfigStr(root, "nvs", section, name);
		cJSON_Delete(root);
	}

	if (ret == NULL) {
		cJSON *root = cJSON_Parse(config_json_start);
		ret = readJsonConfigStr(root, "default", section, name);
		cJSON_Delete(root);
	}

	*dest = ret;
	return ret != NULL ? ESP_OK : !ESP_OK;
}

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
