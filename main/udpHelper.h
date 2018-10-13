/*
 * udpHelper.h
 *
 *  Created on: 06.10.2018
 *      Author: marco
 */

#ifndef MAIN_UDPHELPER_H_
#define MAIN_UDPHELPER_H_

#ifdef __cplusplus
extern "C" {
#endif

#define EXAMPLE_DEFAULT_SERVER_IP "192.168.4.1"
#define EXAMPLE_DEFAULT_PKTSIZE 1024
#define EXAMPLE_PACK_BYTE_IS 0

#define UDP_CONNCETED_SUCCESS BIT1


extern EventGroupHandle_t udp_event_group;

//create a udp server socket. return ESP_OK:success ESP_FAIL:error
esp_err_t create_udp_server(uint16_t port);
//create a udp client socket. return ESP_OK:success ESP_FAIL:error
esp_err_t create_udp_client(const char * ipaddr,uint16_t port);

//send or recv data task
void send_recv_data(void *pvParameters);

//get socket error code. return: error code
int get_socket_error_code(int socket);

//show socket error code. return: error code
int show_socket_error_reason(int socket);

//check connected socket. return: error code
int check_connected_socket();

//close all socket
void close_socket();

#ifdef __cplusplus
}
#endif


#endif /* MAIN_UDPHELPER_H_ */
