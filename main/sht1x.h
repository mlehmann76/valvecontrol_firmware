/*
 * sht1x.h
 *
 *  Created on: 09.04.2019
 *      Author: marco
 */

#ifndef MAIN_SHT1X_H_
#define MAIN_SHT1X_H_

#include <hal/gpio_types.h>

#define SHT1X_MEASURE_TEMP 0x03
#define SHT1X_MEASURE_HUM 0x05
#define SHT1X_READ_STATUS 0x07
#define SHT1X_WRITE_STATUS 0x06
#define SHT1X_RESET 0x1E
#define SHT1X_MEASURE_TIME 320

#define I2C_MASTER_TX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE 0 /*!< I2C master doesn't need buffer */
#define WRITE_BIT I2C_MASTER_WRITE  /*!< I2C master write */
#define READ_BIT I2C_MASTER_READ    /*!< I2C master read */
#define ACK_CHECK_EN 0x1            /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0           /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x1                 /*!< I2C ack value */
#define NACK_VAL 0x0                /*!< I2C nack value */

typedef struct {
	gpio_num_t i2c_gpio_sda;
	gpio_num_t i2c_gpio_scl;
	SemaphoreHandle_t sem;
	float temp;
	float hum;
	bool hasError;
} sht1x_handle_t;


void addSHT1xStatus(cJSON *root);
void setupSHT1xTask(void);

#endif /* MAIN_SHT1X_H_ */
