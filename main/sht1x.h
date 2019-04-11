/*
 * sht1x.h
 *
 *  Created on: 09.04.2019
 *      Author: marco
 */

#ifndef MAIN_SHT1X_H_
#define MAIN_SHT1X_H_

#define SHT1X_MEASURE_TEMP 0x03
#define SHT1X_MEASURE_HUM 0x05
#define SHT1X_READ_STATUS 0x07
#define SHT1X_WRITE_STATUS 0x06

#define SHT1X_MEASURE_TIME 320

void addSHT1xStatus(cJSON *root);
void setupSHT1xTask(void);

#endif /* MAIN_SHT1X_H_ */
