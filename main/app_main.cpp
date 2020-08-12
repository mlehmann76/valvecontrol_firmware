#include <string.h>
#include <stdlib.h>

#include "sdkconfig.h"

#include "SemaphoreCPP.h"
#include "QueueCPP.h"
#include "TimerCPP.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wps.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "esp_task_wdt.h"
#include "esp_http_server.h"
#include "mqtt_client.h"
#include "cJSON.h"

#include "config.h"
#include "config_user.h"

#include "MainClass.h"


#ifdef __cplusplus
extern "C" {
#endif
void app_main();
#ifdef __cplusplus
}
#endif

void app_main() {
	MainClass::instance()->loop();
}


