/*
 * mqtt_user_ota.c
 *
 *  Created on: 29.06.2018
 *      Author: marco
 */

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "esp_flash_partitions.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "sdkconfig.h"

#include "QueueCPP.h"
#include "SemaphoreCPP.h"
#include "TimerCPP.h"
#include "freertos/event_groups.h"

#include "base85.h"
#include "config.h"
#include "config_user.h"
#include "otaWorker.h"

#define TAG "OTA"

namespace Ota {

void B85decode::init() {
    memset(buffer, 0, DECODEBUFSIZE);
    memset(decoded, 0, DECODEBUFSIZE);
    _decodePos = 0;
    _sumLen = 0;
    _len = 0;
}

int B85decode::decode(const uint8_t *src, size_t len) {
    _len = 0;
    int decLen = ((len + _decodePos) / 5) * 5;
    int decRest = (len + _decodePos) % 5;
    if ((len < (DECODEBUFSIZE)-_decodePos)) {
        // copy data to buffer
        memcpy(&buffer[_decodePos], src, len);
        // decode
        int ret =
            decode_85((char *)decoded, (const char *)buffer, decLen / 5 * 4);

        if (ret == 0) {
            // copy rest to buffer
            for (int i = 0; i < decRest; i++) {
                buffer[i] = buffer[i + decLen];
            }
            _decodePos = decRest;
            _len = decLen / 5 * 4;
            _sumLen += decLen / 5 * 4;
            log_inst.debug(TAG, "decode %d", _len);
        } else {
            log_inst.error(TAG, "decode error %d", ret);
        }
    }

    return _len;
}

OtaWorker::OtaWorker()
    : m_timeout("otaTimer", this, &OtaWorker::task,
                (10000 / portTICK_PERIOD_MS), false),
      m_decodeCtx(nullptr) {
    mbedtls_md5_init(&m_md5ctx);
}

int OtaWorker::handle(const OtaPacket &_p) {
    int ret = 0;
    if (m_decodeCtx != nullptr && m_ota_state != OTA_IDLE) {
        int len = m_decodeCtx->decode((uint8_t *)_p.buf(), _p.len());
        if (len > 0) {
            mbedtls_md5_update(&m_md5ctx, m_decodeCtx->data(), len);
        }
        task();
        ret = 1;
    }
    return ret;
}

void OtaWorker::otaFinish() {
    esp_err_t err;
    unsigned char output[16];
    mbedtls_md5_finish(&m_md5ctx, output);
    //    log_inst.debug(TAG,
    //             "fileSize :%d ->md5: "
    //             "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
    //             sum, output[0], output[1], output[2], output[3], output[4],
    //             output[5], output[6], output[7], output[8], output[9],
    //             output[10], output[11], output[12], output[13], output[14],
    //             output[15]);
    if (memcmp(m_md5Updata.md5, output, 16) != 0) {
        log_inst.error(TAG, "md5 sum failed ");
    } else {
        if (esp_ota_end(update_handle) != ESP_OK) {
            log_inst.error(TAG, "esp_ota_end failed!");
        }
        err = esp_ota_set_boot_partition(update_partition);
        if (err != ESP_OK) {
            log_inst.error(TAG, "esp_ota_set_boot_partition failed %s!",
                           esp_err_to_name(err));
        }
        log_inst.info(TAG, "Prepare to restart system!");
        cleanUp();
        esp_restart();
    }
}

void OtaWorker::otaStart() {
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_err_t err;

    if (configured != running) {
        log_inst.warning(
            TAG,
            "Configured OTA boot partition at offset 0x{:08x}, but running "
            "from offset 0x{:08x}",
            configured->address, running->address);
        log_inst.warning(TAG,
                         "(This can happen if either the OTA boot data or "
                         "preferred boot image become corrupted somehow.)");
    }
    log_inst.info(TAG, "Running partition type %d subtype %d (offset 0x{:08x})",
                  running->type, running->subtype, running->address);
    update_partition = esp_ota_get_next_update_partition(NULL);
    log_inst.info(TAG, "Writing to partition subtype %d at offset 0x%x",
                  update_partition->subtype, update_partition->address);
    assert(update_partition != NULL);
    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK) {
        log_inst.error(TAG, "esp_ota_begin failed %s", esp_err_to_name(err));
        cleanUp();
    }
    log_inst.info(TAG, "esp_ota_begin succeeded");

    m_ota_state = OTA_DATA;
}

void OtaWorker::otaData() {
    esp_err_t err;
    m_timeout.reset();
    if (m_decodeCtx != nullptr && m_decodeCtx->len() > 0) {
        sum += m_decodeCtx->len();
        err = esp_ota_write(update_handle, (const void *)(m_decodeCtx->data()),
                            m_decodeCtx->len());
        if (err != ESP_OK) {
            log_inst.error(TAG, "esp_ota_write failed %s",
                           esp_err_to_name(err));
            cleanUp();
        } else {
            log_inst.info(TAG, "esp_ota_write %d%%",
                          sum * 100 / m_md5Updata.len);
        }
        if (sum == m_md5Updata.len) {
            m_ota_state = OTA_FINISH;
            task();
        }
    } else {
        m_ota_state = OTA_ERROR;
    }
}

void OtaWorker::start(const md5_update &_d) {
    log_inst.info(TAG, "starting ota");
    m_decodeCtx = new B85decode(); // TODO make different Decoder possible
    m_md5Updata = _d;
    m_ota_state = OTA_START;
    mbedtls_md5_init(&m_md5ctx);
    mbedtls_md5_starts(&m_md5ctx);
    m_decodeCtx->init();
    m_isRunning = true;
    m_timeout.start();
    // while(!m_timeout.active()) {}
    sum = 0;
    task();
}

void OtaWorker::task() {
    /*
    if (m_ota_state != OTA_IDLE && !m_timeout.active()) {
            ESP_LOGE(TAG, "timeout in ota task");
            m_ota_state = OTA_ERROR;
    }
    */

    switch (m_ota_state) {
    case OTA_IDLE:
        break;

    case OTA_START:
        otaStart();
        break;

    case OTA_DATA:
        otaData();
        break;

    case OTA_FINISH:
        otaFinish();
        break;

    case OTA_ERROR:
        cleanUp();
        m_ota_state = OTA_IDLE;
        break;
    }
}

void OtaWorker::cleanUp() { delete m_decodeCtx; }

} // namespace Ota
