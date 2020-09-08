/*
 * utilities.cpp
 *
 *  Created on: 06.08.2020
 *      Author: marco
 */

#include <memory>
#include "utilities.h"

namespace utilities {

//https://stackoverflow.com/questions/236129/how-do-i-iterate-over-the-words-of-a-string
std::vector<std::string> split(const std::string &text, const std::string &delims) {
	std::vector<std::string> tokens;
	std::size_t start = text.find_first_not_of(delims), end = 0;
	while ((end = text.find_first_of(delims, start)) != std::string::npos) {
		tokens.push_back(text.substr(start, end - start));
		start = text.find_first_not_of(delims, end);
	}
	if (start != std::string::npos)
		tokens.push_back(text.substr(start));

	return tokens;
}



} //namespace utilities


