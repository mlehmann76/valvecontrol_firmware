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

} // namespace utilities

#endif /* MAIN_UTILITIES_H_ */
