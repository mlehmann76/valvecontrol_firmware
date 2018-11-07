/*
 * http_server.c
 *
 *  Created on: 29.10.2018
 *      Author: marco
 */
#include <string.h>
#include <stdlib.h>

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_wps.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_task_wdt.h"
#include "cJSON.h"

#include "mqtt_config.h"
#include "mqtt_client.h"
#include "mqtt_user.h"
#include "mqtt_user_ota.h"

#include "esp_http_server.h"
#include "http_config_server.h"

#include "controlTask.h"

#define TAG "HTTP"


httpd_handle_t start_webserver(void);
void stop_webserver(httpd_handle_t server);

const static char http_index_hml[] =
		"<!DOCTYPE html>"
		"<html>"
		"<body>"
		"</body>"
		"</html>";

/* An HTTP GET handler */
esp_err_t _get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    httpd_resp_set_type(req, HTTPD_TYPE_TEXT);

    const char* resp_str = (const char*) http_index_hml;
    httpd_resp_send(req, resp_str, strlen(resp_str));

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now.
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    */
    return ESP_OK;
}

httpd_uri_t hello = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = _get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
};

/* An HTTP GET handler */
esp_err_t _post_handler(httpd_req_t *req) {
	ESP_LOGI(TAG, "/echo handler read content length %d", req->content_len);

	char* buf = malloc(req->content_len + 1);
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
		ESP_LOGI(TAG, "/echo handler recv length %d", ret);
	}
	buf[off] = '\0';

	if (req->content_len < 128) {
		ESP_LOGI(TAG, "/echo handler read %s", buf);
	}

	httpd_resp_set_type(req, HTTPD_TYPE_TEXT);
	httpd_resp_send(req, buf, 0);
	free(buf);
	return ESP_OK;
}

httpd_uri_t post = {
    .uri       = "/echo",
    .method    = HTTP_POST,
    .handler   = _post_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
};

/* An HTTP GET handler */
esp_err_t _config_handler(httpd_req_t *req) {
	ESP_LOGI(TAG, "/config handler read content length %d", req->content_len);

	char* buf = malloc(req->content_len + 1);
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

	/* read mqtt config */
	cJSON *root = cJSON_Parse(buf);
	cJSON *mqtt_config = cJSON_GetObjectItem(root, "mqtt");

	if (mqtt_config != NULL) {
		setMqttConfig(mqtt_config);
	}

	cJSON_Delete(root);

	httpd_resp_set_type(req, HTTPD_TYPE_JSON);
	httpd_resp_send(req, buf, 0);
	free(buf);
	return ESP_OK;
}

httpd_uri_t put_config = {
    .uri       = "/config",
    .method    = HTTP_PUT,
    .handler   = _config_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
};

httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &hello);
        httpd_register_uri_handler(server, &post);
        httpd_register_uri_handler(server, &put_config);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}




