/*
 * logger.h
 *
 *  Created on: 12.10.2020
 *      Author: marco
 */

#ifndef COMPONENTS_LOGGER_LOGGER_H_
#define COMPONENTS_LOGGER_LOGGER_H_

#include "../logger/logger_impl.h"
#include "../logger/logger_policy.h"

namespace logger {

template <                                  //
    typename log_policy = DefaultLogPolicy, //
    typename option_traits =
        ColoredOutput<severity_type::debug, severity_type::debug> //
    >                                                             //
class Logger {

public:
  Logger(std::string name)
      : m_impl(std::make_unique<detail::LoggerImpl<log_policy, option_traits>>(
            std::move(name))) {}
  /**
   *
   */
  template <typename C, typename S, typename... Args>
  void debug(const C &tag, const S &format_str, Args &&... args) {
    if (option_traits().min >= severity_type::debug)
      m_impl->print(detail::sdebug(), tag, format_str,
                    std::forward<Args>(args)...);
  }
  /**
   *
   */
  template <typename C, typename S, typename... Args>
  void info(const C &tag, const S &format_str, Args &&... args) {
    if (option_traits().min >= severity_type::info)
      m_impl->print(detail::sinfo(), tag, format_str,
                    std::forward<Args>(args)...);
  }
  /**
   *
   */
  template <typename C, typename S, typename... Args>
  void error(const C &tag, const S &format_str, Args &&... args) {
    if (option_traits().min >= severity_type::error)
      m_impl->print(detail::serror(), tag, format_str,
                    std::forward<Args>(args)...);
  }
  /**
   *
   */
  template <typename C, typename S, typename... Args>
  void warning(const C &tag, const S &format_str, Args &&... args) {
    if (option_traits().min >= severity_type::warning)
      m_impl->print(detail::swarning(), tag, format_str,
                    std::forward<Args>(args)...);
  }
  /**
   *
   */
  log_policy *policy() const { return m_impl->policy(); }
  /**
   *
   */
  template <typename C> void setLogSeverity(const C &tag, severity_type sev) {
    m_impl->setLogSeverity(tag, sev);
  }

private:
  std::unique_ptr<detail::LoggerImpl<log_policy, option_traits>> m_impl;
};
} // namespace logger

#endif /* COMPONENTS_LOGGER_LOGGER_H_ */
