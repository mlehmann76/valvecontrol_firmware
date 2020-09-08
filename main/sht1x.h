/*
 * sht1x.h
 *
 *  Created on: 09.04.2019
 *      Author: marco
 */

#ifndef MAIN_SHT1X_H_
#define MAIN_SHT1X_H_

#include "hal/gpio_types.h"
#include "statusTask.h"

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

class Sht1x {
public:

	Sht1x(gpio_num_t _sda, gpio_num_t _scl);
	virtual ~Sht1x() = default;
	void reset();
	//
private:
	float readSHT1xTemp();
	float readSHT1xHum();
	bool hasError() const {return m_error; }

	void delay_usec(int64_t usec) {
		int64_t end = esp_timer_get_time() + usec;
		while (esp_timer_get_time() < end) {	};
	}

	void _sda_(int lvl);
	void _scl_(int lvl);
	void start();
	void connectionreset();
	uint32_t read(uint32_t numBits, int ack);
	int write(uint32_t data);
	esp_err_t readSHT1xReg16(uint8_t reg, uint16_t *pData);
	void readSensor();

private:
	TimerMember<Sht1x> m_timeout;
	gpio_num_t i2c_gpio_sda;
	gpio_num_t i2c_gpio_scl;
	float m_hum, m_temp;
	bool m_error;
	bool m_update;
	Semaphore m_sem;
};

#endif /* MAIN_SHT1X_H_ */
