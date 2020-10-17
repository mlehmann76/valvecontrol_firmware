/*
 * logger.h
 *
 *  Created on: 12.10.2020
 *      Author: marco
 */

#ifndef COMPONENTS_LOGGER_LOGGER_IMPL_H_
#define COMPONENTS_LOGGER_LOGGER_IMPL_H_

#include <fmt/color.h>
#include <fmt/format.h>

#include <mutex>
#include <sstream>
#include <unordered_map>

#include "../logger/logger_policy.h"

namespace logger {

enum class severity_type { error, warning, info, debug };

namespace detail {

template <                 //
    typename log_policy,   //
    typename option_traits //
    >                      //
class LoggerImpl {

  struct severity {
    constexpr severity(severity_type v = option_traits().def) : value(v) {}
    severity_type value;
  };

  using mapType = std::unordered_map<std::string, severity>;

public:
  template <class T, typename C, typename S, typename... Args>
  void print(const T &severity, const C &tag, const S &format_str,
             Args &&... args) {
    if (severity.type <= m_map[tag].value) {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_stream << std::chrono::duration_cast<std::chrono::microseconds>(
                      std::chrono::steady_clock::now() - m_startTime)
                      .count()
               << " : ";
      m_stream << severity.name << " : " << tag << " : ";
      m_stream << fmt::format(format_str, args...);
      m_stream << "\n";
      if (option_traits().echo) {
        if (option_traits().colored)
          fmt::print(fg(severity.color), m_stream.str());
        else
          fmt::print(m_stream.str());
      }
      print_impl();
    }
  }

  LoggerImpl(std::string name)
      : m_policy(std::make_unique<log_policy>()),
        m_startTime(std::chrono::steady_clock::now()) {
    m_policy->open(std::move(name));
  }

  LoggerImpl(const LoggerImpl &) = delete;
  LoggerImpl(LoggerImpl &&) = delete;
  LoggerImpl &operator=(const LoggerImpl &) = delete;

  ~LoggerImpl() { m_policy->close(); }

  log_policy *policy() const { return m_policy.get(); }

  template <typename C> void setLogSeverity(const C &tag, severity_type sev) {
    m_map[tag] = sev;
  }

private:
  void print_impl() {
    m_policy->write(m_stream.str());
    m_stream.str("");
  }

  template <typename First, typename... Rest>
  void print_impl(First parm1, Rest... parm) {
    m_stream << parm1;
    print_impl(parm...);
  }

  std::stringstream m_stream;
  std::unique_ptr<log_policy> m_policy;
  std::mutex m_mutex;
  std::chrono::steady_clock::time_point m_startTime;
  mapType m_map;
};

/**
 * traits
 */
template <severity_type s, char *n, fmt::color c> struct severity_trait {
  static constexpr severity_type type = s;
  static constexpr const char *name = n;
  static constexpr fmt::color color = c;
};

template <severity_type _d, severity_type _m, bool _e, bool _c>
struct option_traits {
  static constexpr severity_type def = _d;
  static constexpr severity_type min = _m;
  static constexpr bool echo = _e;
  static constexpr bool colored = _c;
};

namespace {
char debug_string[] = "DEBUG";
char error_string[] = "ERROR";
char warning_string[] = "WARNING";
char info_string[] = "INFO";
} // namespace

/**
 * traits specialisation
 */
using sdebug = detail::severity_trait< //
    severity_type::debug, detail::debug_string, fmt::color::white>;
using serror = detail::severity_trait< //
    severity_type::error, detail::error_string, fmt::color::red>;
using swarning = detail::severity_trait< //
    severity_type::warning, detail::warning_string, fmt::color::orange>;
using sinfo = detail::severity_trait< //
    severity_type::info, detail::info_string, fmt::color::green>;
} // namespace detail

template <severity_type _d, severity_type _m>
using NoOutput = detail::option_traits<_d, _m, false, false>;

template <severity_type _d, severity_type _m>
using ColoredOutput = detail::option_traits<_d, _m, true, true>;

template <severity_type _d, severity_type _m>
using UncoloredOutput = detail::option_traits<_d, _m, true, false>;

} // namespace logger

#endif /* COMPONENTS_LOGGER_LOGGER_IMPL_H_ */
