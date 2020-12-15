/*
 * log_policy.h
 *
 *  Created on: 12.10.2020
 *      Author: marco
 */

#ifndef COMPONENTS_LOGGER_LOGGER_POLICY_H_
#define COMPONENTS_LOGGER_LOGGER_POLICY_H_

#include <fmt/color.h>
#include <fmt/format.h>
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
            fmt::print(fg(fmt::color::white), msg);
            break;
        case severity_type::info:
            fmt::print(fg(fmt::color::green), msg);
            break;
        case severity_type::error:
            fmt::print(fg(fmt::color::red), msg);
            break;
        case severity_type::warning:
            fmt::print(fg(fmt::color::orange), msg);
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
