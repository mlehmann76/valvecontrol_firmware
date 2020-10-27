/*
 * circular_policy.h
 *
 *  Created on: 26.10.2020
 *      Author: marco
 */

#ifndef LOGGER_CIRCULAR_POLICY_H_
#define LOGGER_CIRCULAR_POLICY_H_

#include <circular_buffer.h>
#include <logger_def.h>
#include <logger_policy.h>

namespace logger {

template <size_t N> struct CircularLogPolicy : public iLogPolicy {
    CircularLogPolicy() : m_cb() {}
    void open() override {}
    void close() override {}
    void write(severity_type s, const std::string &msg) override {
        m_cb.push_back(msg);
    }
    circular_buffer<std::string, N> m_cb;
};

} // namespace logger

#endif /* LOGGER_CIRCULAR_POLICY_H_ */
