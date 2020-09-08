/*
 * utilities.h
 *
 *  Created on: 06.08.2020
 *      Author: marco
 */

#ifndef MAIN_UTILITIES_H_
#define MAIN_UTILITIES_H_

#include <string>
#include <memory>
#include <vector>
namespace utilities {

std::vector<std::string> split(const std::string &text, const std::string &delims);

template<typename ... Args>
std::string string_format(const std::string &format, Args ... args) {
	size_t size = snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
	if (size > 0) {
		std::unique_ptr<char[]> buf(new char[size]);
		snprintf(buf.get(), size, format.c_str(), args ...);
		return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
	} else {
		return std::string("");
	}
}


} //namespace utilities



#endif /* MAIN_UTILITIES_H_ */
