/*
 * config.h
 *
 *  Created on: 28.04.2019
 *      Author: marco
 */

#ifndef COMPONENTS_JSONCONFIG_JSONCONFIG_H_
#define COMPONENTS_JSONCONFIG_JSONCONFIG_H_

#include "esp_err.h"

esp_err_t configInit(void);
esp_err_t readConfigStr(const char* section, const char* name, char **dest);
void updateConfig(const char *pjsonConfig);

#endif /* COMPONENTS_JSONCONFIG_JSONCONFIG_H_ */
