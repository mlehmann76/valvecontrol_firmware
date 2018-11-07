/*
 * mqtt_config.h
 *
 *  Created on: 27.10.2018
 *      Author: marco
 */

#ifndef MAIN_MQTT_CONFIG_H_
#define MAIN_MQTT_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Constants that aren't configurable in menuconfig */
#define MQTT_SERVER "mqtt://raspberrypi.fritz.box"
#define MQTT_DEVICE "esp32/"
#define MQTT_USER "sensor1"
#define MQTT_PASS "sensor1"

#define MQTT_PUB_MESSAGE_FORMAT "%s%02X%02X%02X%02X%02X%02X%s"

/*
enum {eUINT8, eINT8, eUINT16, eINT16, eUINT32, eINT32, eSTR, eBLOB} nvs_type;
typedef enum nvs_type nvs_type_t;
*/

const char* getSubMsg();
const char* getPubMsg();
const char* getMqttServer();
const char* getMqttUser();
const char* getMqttPass();
esp_err_t setMqttConfig(cJSON* config);

void mqtt_config_init();

#ifdef __cplusplus
}
#endif


#endif /* MAIN_MQTT_CONFIG_H_ */
