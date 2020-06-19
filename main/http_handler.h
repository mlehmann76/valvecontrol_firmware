/*
 * http_handler.h
 *
 *  Created on: 15.06.2020
 *      Author: marco
 */

#ifndef MAIN_HTTP_HANDLER_H_
#define MAIN_HTTP_HANDLER_H_

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t _get_handler(httpd_req_t *req);
esp_err_t _config_handler(httpd_req_t *req);
esp_err_t _command_handler(httpd_req_t *req);

#ifdef __cplusplus
}
#endif


#endif /* MAIN_HTTP_HANDLER_H_ */
