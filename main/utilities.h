/*
 * utilities.h
 *
 *  Created on: 06.08.2020
 *      Author: marco
 */

#ifndef MAIN_UTILITIES_H_
#define MAIN_UTILITIES_H_

#include <memory>
#include <string>
#include <vector>

namespace utilities {

std::vector<std::string> split(const std::string &text,
                               const std::string &delims);

std::string toHex(const std::string& in) ;
std::string fromHex(const std::string& in) ;
std::string base64_encode(const std::string &in);
std::string base64_decode(const std::string &in);

} // namespace utilities

#endif /* MAIN_UTILITIES_H_ */
