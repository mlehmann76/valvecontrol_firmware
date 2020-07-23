/*
 * http_handler.c
 *
 *  Created on: 15.06.2020
 *      Author: marco
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "sdkconfig.h"

#include "TaskCPP.h"
#include "SemaphoreCPP.h"
#include "QueueCPP.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wps.h"
#include "nvs_flash.h"
#include "esp_task_wdt.h"
#include "cJSON.h"

extern "C" {
#include "../esp-idf/components/wpa_supplicant/src/utils/base64.h"
}

#include "config.h"
#include "mqtt_client.h"
#include "mqttUserTask.h"
#include "statusTask.h"

#include "esp_https_server.h"
#include "controlTask.h"

#include "http_handler.h"
#include "http_server.h"

#define TAG "HTTP"
/***
 *
 */
static const char* fileEndings[] = {".html",".css",".js"};
#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

static const char AUTHORIZATION_HEADER[] = "Authorization";
static const char WWW_Authenticate[] = "WWW-Authenticate";
const static char BASIC[] = "Basic";
const static char basicRealm[] = "Basic realm=\"";
const static char digestRealm[] = "Digest realm=\"";

/**
 *
 */
static char nonce[33];
static char opaque[33];
static char pbrealm[256];

/**
 *
 */
static int _pos(const char* s, size_t s_len, const char p) {
	int ret = -1;
	for (int i=0;i<s_len;i++) {
		if (s[i] == p) {
			ret = i;
			break;
		}
	}
	return ret;
}

static void _getRandomHexString(char *buf, size_t len) {
  for(int i = 0; i < 4; i++) {
    snprintf (buf + (i*8), len, "%08x", esp_random());
    len-=8;
  }
}

void requestAuth(httpd_req_t *r, HTTPAuthMethod mode, const char *realm, const char *failMsg) {
	if (realm == NULL) {
		realm = "Login Required";
	}
	if (mode == BASIC_AUTH) {
		snprintf(pbrealm,sizeof(pbrealm),"%s%s\"",basicRealm,realm);
	} else {
		_getRandomHexString(nonce, sizeof(nonce));
		_getRandomHexString(opaque, sizeof(opaque));
		snprintf(pbrealm,sizeof(pbrealm),"%s%s\", qop=\"auth\", nonce=\"%s\", opaque=\"%s\"",
				digestRealm,realm,nonce,opaque);
	}
	httpd_resp_set_hdr(r, WWW_Authenticate, pbrealm);
	httpd_resp_set_status(r, "401 Unauthorized");
	// End response
	httpd_resp_send_chunk(r, NULL, 0);
}


static esp_err_t authenticate(char *buf, size_t buf_len, const char* pUser, const char* pPass) {
	char mode[32];
	char encoded[128];
	unsigned char *pdecode;
	esp_err_t ret = ESP_FAIL;
	size_t outlen = 0;
	sscanf(buf, "%32s %128s", mode, encoded);
	ESP_LOGI(TAG, "Found header => Authorization: mode %s pass %s", mode, encoded);
	//BASIC Mode
	if (0 == strncmp(mode, BASIC, sizeof(BASIC))) {
		pdecode = base64_decode(encoded, strnlen(encoded, sizeof(encoded)), &outlen);
		if (outlen && pUser!=NULL && pPass!=NULL) {
			char user[128];
			char passw[128];
			int posc = _pos((char*)pdecode, outlen, ':');
			if (posc != -1) {
				memcpy(user,pdecode,posc); user[posc]=0;
				memcpy(passw, &pdecode[posc+1], outlen-posc); passw[outlen-posc]=0;
			}
			ESP_LOGI(TAG, "Basic Authorization: %s,%s", user, passw);
			if ((0==strncmp(user,pUser,max(strlen(user),strlen(pUser)))) &&
					(0==strncmp(passw,pPass,max(strlen(passw),strlen(pPass))))) {
				ESP_LOGI(TAG, "Basic Authorization OK");
				ret = ESP_OK;
			}
		}
		free(pdecode);
	}
	return ret;
}

esp_err_t _send_file(httpd_req_t *req, const char *fname) {

    FILE* f = fopen(fname, "r");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open %s", fname);
        return ESP_FAIL;
    }

    char buf[64];
    while (!feof(f)) {
    	memset(buf, 0, sizeof(buf));
    	size_t len = fread(buf, 1, sizeof(buf), f);
    	httpd_resp_send_chunk(req, buf, len);
    }
    httpd_resp_send_chunk(req, NULL, 0);
    fclose(f);
    return ESP_OK;
}
/**
 *
 */
esp_err_t _fileHandler(httpd_req_t *req) {
	esp_err_t ret = ESP_OK;
	size_t maxSize = HTTPD_MAX_URI_LEN + strlen("/spiffs/") + 1;
	char *pBuf = (char*)malloc(maxSize);
	if (pBuf != NULL) {
		snprintf(pBuf, maxSize, "/spiffs/%s", req->uri);
		ret = _send_file(req, pBuf);
		free(pBuf);
	} else {
		ret = ESP_FAIL;
	}
	return ret;
}
/**
 *
 */
static bool _isFile(const char *filename,size_t namelen) {
	bool ret = false;
	for (size_t si=0;si<(sizeof(fileEndings)/sizeof(*fileEndings));si++) {
		size_t endLen= strnlen(fileEndings[si],HTTPD_MAX_URI_LEN);
		ESP_LOGI(TAG, "isFile %s / %s", &filename[namelen-endLen], fileEndings[si]);
		if (0 == strcasecmp(&filename[namelen-endLen], fileEndings[si])) {
			ret = true;
			break;
		}
	}
	return ret;
}
/**
 * An HTTP GET handler
 */
esp_err_t _get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;
    esp_err_t ret = ESP_OK;

    buf_len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;
    if (buf_len > 1) {
    	buf = (char*)malloc(buf_len);
		/* Copy null terminated value string into buffer */
		if (httpd_req_get_hdr_value_str(req, AUTHORIZATION_HEADER, buf, buf_len) == ESP_OK) {
			const char *pUser = sysConf.getUser();
			const char *pPass = sysConf.getPass();
			if(authenticate(buf, buf_len, pUser, pPass) == ESP_OK) {

			} else {
		    	requestAuth(req, BASIC_AUTH, "esp32", "Login failed");
		    	ESP_LOGI(TAG, "Authorization requested");
		    	return ESP_OK;
			}
		}
		free(buf);
    } else {
    	requestAuth(req, BASIC_AUTH, "esp32", "Login failed");
    	ESP_LOGI(TAG, "Authorization requested");
    	return ESP_OK;
    }

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        // Copy null terminated value string into buffer
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }
	*/

    if (_isFile(req->uri, strnlen(req->uri,HTTPD_MAX_URI_LEN))) {
    	ret = _fileHandler(req);
    } else {
		httpd_resp_set_type(req, HTTPD_TYPE_JSON);

		//char *out = sys.getConfigJson();
		//httpd_resp_send(req, out, strlen(out));
		//free(out);
	}

    return ret;
}


/* An HTTP POST handler */
esp_err_t _config_handler(httpd_req_t *req) {
	ESP_LOGI(TAG, "/config handler read content length %d", req->content_len);

	char* buf = (char*)malloc(req->content_len + 1);
	size_t off = 0;
	int ret;

	if (!buf) {
		httpd_resp_set_status(req, HTTPD_500);
		return ESP_FAIL;
	}

	while (off < req->content_len) {
		/* Read data received in the request */
		ret = httpd_req_recv(req, buf + off, req->content_len - off);
		if (ret <= 0) {
			{
				httpd_resp_set_status(req, HTTPD_500);
			}
			free(buf);
			return ESP_FAIL;
		}
		off += ret;
		ESP_LOGI(TAG, "/config handler recv length %d", ret);
	}
	buf[off] = '\0';

	if (req->content_len < 128) {
		ESP_LOGI(TAG, "/config handler read %s", buf);
	}

	/* read mqtt config FIXME
	updateConfig(buf);
*/
	httpd_resp_set_type(req, HTTPD_TYPE_JSON);
	httpd_resp_send(req, buf, 0);
	free(buf);
	return ESP_OK;
}


/* An HTTP GET handler */
esp_err_t _command_handler(httpd_req_t *req) {
	ESP_LOGI(TAG, "/command handler read content length %d", req->content_len);

	char* buf = (char*)malloc(req->content_len + 1);
	size_t off = 0;
	int ret;

	if (!buf) {
		httpd_resp_set_status(req, HTTPD_500);
		return ESP_FAIL;
	}

	while (off < req->content_len) {
		/* Read data received in the request */
		ret = httpd_req_recv(req, buf + off, req->content_len - off);
		if (ret <= 0) {
			httpd_resp_set_status(req, HTTPD_500);
			free(buf);
			return ESP_FAIL;
		}
		off += ret;
		ESP_LOGI(TAG, "/command handler recv length %d", ret);
	}
	buf[off] = '\0';

	if (req->content_len < 128) {
		ESP_LOGI(TAG, "/command handler read %s", buf);
	}

	/* read mqtt config */
	cJSON *root = cJSON_Parse(buf);
	cJSON *command_str = cJSON_GetObjectItem(root, "channel");

	if (command_str != NULL) {
		//handleChannelControl(command_str);
	}

	cJSON_Delete(root);

	httpd_resp_set_type(req, HTTPD_TYPE_JSON);
	httpd_resp_send(req, buf, 0);
	free(buf);
	return ESP_OK;
}



