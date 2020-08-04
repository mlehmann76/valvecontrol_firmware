/*
 * http_server.c
 *
 *  Created on: 29.10.2018
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
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wps.h"
#include "nvs_flash.h"
#include "esp_task_wdt.h"
#include "cJSON.h"

#include "config.h"
#include "mqtt_config.h"
#include "mqtt_client.h"
#include "mqttUserTask.h"
#include "esp_http_server.h"

#include "http_handler.h"
#include "http_server.h"
#include "statusTask.h"

#define TAG "HTTP"



httpd_uri_t get_base = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = _get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
	.user_ctx = NULL
};


httpd_uri_t put_config = {
    .uri       = "/config",
    .method    = HTTP_PUT,
    .handler   = _config_handler,
	.user_ctx = NULL
};


httpd_uri_t put_command = {
    .uri       = "/command",
    .method    = HTTP_PUT,
    .handler   = _command_handler,
	.user_ctx = NULL
};



httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
/*
    extern const unsigned char cacert_pem_start[] asm("_binary_cacert_pem_start");
    extern const unsigned char cacert_pem_end[]   asm("_binary_cacert_pem_end");

    extern const unsigned char prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
    extern const unsigned char prvtkey_pem_end[]   asm("_binary_prvtkey_pem_end");
*/
    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &get_base);
        httpd_register_uri_handler(server, &put_command);
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




