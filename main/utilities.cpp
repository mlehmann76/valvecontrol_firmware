/*
 * utilities.cpp
 *
 *  Created on: 06.08.2020
 *      Author: marco
 */

#include "utilities.h"
#include <memory>
#include <sstream>
#include <iomanip>
#include <cassert>

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

std::string toHex(const std::string &in) {
	std::stringstream _s;
	for (const auto &e : in) {
		_s << std::uppercase << std::setfill('0') << std::setw(2) << std::hex
				<< unsigned(e);
	}
	return _s.str();
}

std::string fromHex(const std::string &in) {
	assert(in.length() % 2 == 0);
	std::string _out;
	for (size_t i = 0; i < in.length() / 2; i++) {
		unsigned x;
		std::stringstream ss;
		ss << std::hex << in.substr(2 * i, 2);
		ss >> x;
		_out += x;
	}
	return _out;
}

std::string base64_encode(const std::string &in) {

    std::string out;

    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[(val>>valb)&0x3F]);
            valb -= 6;
        }
    }
    if (valb>-6) out.push_back("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[((val<<8)>>(valb+8))&0x3F]);
    while (out.size()%4) out.push_back('=');
    return out;
}

std::string base64_decode(const std::string &in) {

    std::string out;

    std::vector<int> T(256,-1);
    for (int i=0; i<64; i++) T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;

    int val = 0, valb =-8;
    for (unsigned char c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val>>valb)&0xFF));
            valb -= 8;
        }
    }
    return out;
}
} // namespace utilities
