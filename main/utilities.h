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

std::string toHex(const std::string &in);
std::string fromHex(const std::string &in);
std::string base64_encode(const std::string &in);
std::string base64_decode(const std::string &in);

template <typename... Args>
std::string string_format(const std::string &format, Args... args) {
    int size_s = ::snprintf(nullptr, 0, format.c_str(), args...) +
                 1; // Extra space for '\0'
    if (size_s <= 0) {
        throw std::runtime_error("Error during formatting.");
    }
    auto size = static_cast<size_t>(size_s);
    auto buf = std::make_unique<char[]>(size);
    ::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(),
                       buf.get() + size - 1); // We don't want the '\0' inside
}

} // namespace utilities

#endif /* MAIN_UTILITIES_H_ */
