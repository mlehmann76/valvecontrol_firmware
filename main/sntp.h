/*
 * sntp.h
 *
 *  Created on: 27.04.2019
 *      Author: marco
 */

#ifndef MAIN_SNTP_H_
#define MAIN_SNTP_H_

#ifdef __cplusplus
extern "C" {
#endif

extern void sntp_support(void);
extern esp_err_t update_time(void);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_SNTP_H_ */
