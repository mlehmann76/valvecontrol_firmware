/*
 * config.h
 *
 *  Created on: 28.04.2019
 *      Author: marco
 */

#ifndef COMPONENTS_JSONCONFIG_JSONCONFIG_H_
#define COMPONENTS_JSONCONFIG_JSONCONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"

extern esp_err_t configInit(void);
extern esp_err_t readConfigStr(const char* section, const char* name, char **dest);
extern void updateConfig(const char *pjsonConfig);
extern const char* getConfigJson(void);

#ifdef __cplusplus
}
#endif
#endif /* COMPONENTS_JSONCONFIG_JSONCONFIG_H_ */
