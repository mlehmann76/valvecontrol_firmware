/*
 * log_policy.h
 *
 *  Created on: 12.10.2020
 *      Author: marco
 */

#ifndef COMPONENTS_LOGGER_LOGGER_POLICY_H_
#define COMPONENTS_LOGGER_LOGGER_POLICY_H_

#include <fstream>
#include <memory>

#include <logger_def.h>

namespace logger {
/**
 *
 */
struct iLogPolicy {
    virtual ~iLogPolicy() = default;
    virtual void open() = 0;
    virtual void close() = 0;
    virtual void write(severity_type s, const std::string &) = 0;
};
/**
 *
 */
struct DefaultLogPolicy : public iLogPolicy {
    void open() override {}
    void close() override {}
    void write(severity_type s, const std::string &msg) override {
        printf("%s", msg.c_str());
    }
};
/**
 *
 */
struct ColoredTerminal : public iLogPolicy {
    void open() override {}
    void close() override {}
    void write(severity_type s, const std::string &msg) override {
        switch (s) {
        case severity_type::debug:
            printf("\033[0;37m%s",msg.c_str());//White
            break;
        case severity_type::info:
            printf("\033[0;32m%s",msg.c_str());//Green
            break;
        case severity_type::error:
            printf("\033[0;31m%s",msg.c_str());//Red
            break;
        case severity_type::warning:
            printf("\033[0;33m%s",msg.c_str());//Yellow
            break;
        }
    }
};
/**
 *
 */
struct FileLogPolicy : public iLogPolicy {
    FileLogPolicy(std::string name) noexcept
        : out_stream(new std::ofstream), _name(std::move(name)) {}
    void open() override;
    void close() override;
    void write(severity_type s, const std::string &msg) override;
    std::unique_ptr<std::ofstream> out_stream;
    std::string _name;
};

inline void FileLogPolicy::open() { out_stream->open(_name, std::ios::out); }

inline void FileLogPolicy::close() { out_stream->close(); }

inline void FileLogPolicy::write(severity_type, const std::string &msg) {
    out_stream->write(msg.c_str(), static_cast<std::streamsize>(msg.length()));
}

} // namespace logger

#endif /* COMPONENTS_LOGGER_LOGGER_POLICY_H_ */
