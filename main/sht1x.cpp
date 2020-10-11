/*
 * sht1x.c
 *
 *  Created on: 09.04.2019
 *      Author: marco
 */

#include "config_user.h"
#include <time.h>

#include "QueueCPP.h"
#include "SemaphoreCPP.h"
#include "TimerCPP.h"
#include "freertos/event_groups.h"

#include "driver/i2c.h"
#include "esp_log.h"

#include "config.h"
#include "mqtt_client.h"
#include "repository.h"
#include "sht1x.h"

static const char *TAG = "I2C";
static const int _us = 20;

Sht1x::Sht1x(gpio_num_t _sda, gpio_num_t _scl)
    : m_timeout("otaTimer", this, &Sht1x::readSensor,
                (1000 / portTICK_PERIOD_MS), false),
      i2c_gpio_sda(_sda), i2c_gpio_scl(_scl), m_error(false), m_rep(nullptr),
      m_temp(0), m_hum(0) {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT_OUTPUT_OD;
    io_conf.pin_bit_mask = (1 << GPIO_NUM_21) | (1 << GPIO_NUM_22);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;

    gpio_config(&io_conf);
    gpio_set_level(GPIO_NUM_21, 1);
    gpio_set_level(GPIO_NUM_22, 1);
    m_timeout.start();
}

double Sht1x::readSHT1xTemp() {
    static const float d1 = -40.1;
    static const float d2 = 0.01; // 12bit

    if (hasError()) {
        reset();
    }

    uint16_t soth = 0;
    if (readSHT1xReg16(SHT1X_MEASURE_TEMP, &soth) != ESP_OK) {
        soth = 0;
        m_error = true;
    } else {
        m_error = false;
    }

    soth &= 0x3FFF;

    return (d1 + d2 * soth);
}

double Sht1x::readSHT1xHum() {
    static const float c1 = -2.0468;
    static const float c2 = +0.0367;
    static const float c3 = -1.5955E-6;

    if (hasError()) {
        reset();
    }

    uint16_t sorh = 0;
    if (readSHT1xReg16(SHT1X_MEASURE_HUM, &sorh) != ESP_OK) {
        sorh = 0;
        m_error = true;
    } else {
        m_error = false;
    }

    sorh &= 0x0FFF;

    float rth = (c1 + c2 * sorh + c3 * sorh * sorh);

    /* disabled for testing
     // temperature compensation
     if (!pHandle->m_error) {
     rth = (pHandle->temp - 25) * (0.01 + 0.00008 * sorh) + rth;
     }
     */
    return rth;
}

void Sht1x::reset() {
    connectionreset();
    m_error = false;
    write(SHT1X_RESET);
}

void Sht1x::regProperty(repository *rep, const std::string &name) {
    m_rep = rep;
    m_name = name;
    if (m_rep != nullptr) {
        m_rep->create(m_name, {{{"temperature", m_temp}, {"humidity", m_hum}}});
    }
}

void Sht1x::updateProperty() {
    if (m_rep != nullptr) {
        m_rep->set(m_name, {{"temperature", m_temp}, {"humidity", m_hum}});
    }
}

void Sht1x::readSensor() {
    if (hasError()) {
        m_timeout.period(
            std::chrono::duration_cast<portTick>(std::chrono::seconds(60))
                .count());
        m_timeout.start();
        reset();
    } else {
        m_timeout.period(
            std::chrono::duration_cast<portTick>(std::chrono::seconds(10))
                .count());
        m_timeout.start();
        m_temp = readSHT1xTemp();
        m_hum = readSHT1xHum();
        updateProperty();
    }
}

void Sht1x::_sda_(int lvl) { gpio_set_level(i2c_gpio_sda, lvl ? 1 : 0); }

void Sht1x::_scl_(int lvl) {
    gpio_set_level(i2c_gpio_scl, lvl ? 1 : 0);
    delay_usec(_us);
}

void Sht1x::start() {
    // start pattern
    _sda_(1);
    _scl_(0);
    _scl_(1);
    _sda_(0);
    delay_usec(_us);
    _scl_(0);

    _scl_(1);
    _sda_(1);
    delay_usec(_us);
    _scl_(0);
}

void Sht1x::connectionreset() {
    _sda_(1);
    _scl_(0);
    // reset
    for (int i = 0; i < 9; i++) {
        _scl_(1);
        _scl_(0);
    }
    start();
}

uint32_t Sht1x::read(uint32_t numBits, int ack) {
    uint32_t ret = 0;
    _sda_(1);
    for (size_t i = 0; i < numBits; ++i) {
        _scl_(1);
        ret = ret * 2 + gpio_get_level(i2c_gpio_sda);
        _scl_(0);
    }

    _sda_(ack == 0 ? 1 : 0);
    _scl_(1);
    _scl_(0);
    _sda_(1);

    ESP_LOGI(TAG, "read i2c %d", ret);
    return (ret);
}

int Sht1x::write(uint32_t data) {
    int ack;
    for (size_t i = 0; i < 8; ++i) {
        _sda_((data & (1 << (7 - i))) ? 1 : 0);
        _scl_(1);
        _scl_(0);
    }
    _sda_(1);
    _scl_(1);
    ack = gpio_get_level(i2c_gpio_sda);
    _scl_(0);
    return ack;
}

esp_err_t Sht1x::readSHT1xReg16(uint8_t reg, uint16_t *pData) {

    esp_err_t ret = ESP_OK;
    int ack = 0;
    start();
    ack += write(reg);

    if (ack != 0) {
        ESP_LOGI(TAG, "error on ack i2c 1");
        ret = ESP_ERR_TIMEOUT;
    }

    time_t start, now;
    time(&now);
    time(&start);

    while (1) {
        time(&now);
        if (gpio_get_level(i2c_gpio_sda) == 0) {
            break;
        }
        if (difftime(now, start) >= 2) {
            ESP_LOGI(TAG, "timeout on waiting i2c ");
            ret = ESP_ERR_TIMEOUT;
            break;
        }
    }

    uint16_t val = read(8, ACK_VAL);
    val *= 256;
    val |= read(8, ACK_VAL);

    read(8, NACK_VAL);

    *pData = val;

    _sda_(1);
    _scl_(1);
    return ret;
}
