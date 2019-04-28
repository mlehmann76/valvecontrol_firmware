/*
 * sntp.h
 *
 *  Created on: 27.04.2019
 *      Author: marco
 */

#ifndef MAIN_SNTP_H_
#define MAIN_SNTP_H_

void setup_sntp(const char* const pTz);
esp_err_t update_time(void);



#endif /* MAIN_SNTP_H_ */
