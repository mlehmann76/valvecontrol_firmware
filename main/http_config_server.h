/*
 * http_config_server.h
 *
 *  Created on: 29.10.2018
 *      Author: marco
 */

#ifndef MAIN_HTTP_CONFIG_SERVER_H_
#define MAIN_HTTP_CONFIG_SERVER_H_

#ifdef __cplusplus
extern "C" {
#endif

httpd_handle_t start_webserver(void);
void stop_webserver(httpd_handle_t server);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_HTTP_CONFIG_SERVER_H_ */
