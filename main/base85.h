/*
 * base85.h
 *
 *  Created on: 29.06.2018
 *      Author: marco
 */

#ifndef MAIN_BASE85_H_
#define MAIN_BASE85_H_

#ifdef __cplusplus
extern "C" {
#endif

int decode_85(char *dst, const char *buffer, int len);
void encode_85(char *buf, const unsigned char *data, int bytes);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_BASE85_H_ */
