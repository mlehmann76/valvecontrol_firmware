/*
 * utilities.cpp
 *
 *  Created on: 06.08.2020
 *      Author: marco
 */

#include "utilities.h"
#include <memory>

namespace utilities {

// https://stackoverflow.com/questions/236129/how-do-i-iterate-over-the-words-of-a-string
std::vector<std::string> split(const std::string &text,
                               const std::string &delims) {
    std::vector<std::string> tokens;
    std::size_t start = 0, end = 0;

    while ((end = text.find(delims, start)) != std::string::npos) {
        if (end != start)
            tokens.push_back(text.substr(start, end - start));
        start = end + delims.length();
    }
    if (start != std::string::npos)
        tokens.push_back(text.substr(start));

    return tokens;
}

} // namespace utilities
