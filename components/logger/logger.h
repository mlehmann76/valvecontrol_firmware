/*
 * logger.h
 *
 *  Created on: 12.10.2020
 *      Author: marco
 */

#ifndef COMPONENTS_LOGGER_LOGGER_H_
#define COMPONENTS_LOGGER_LOGGER_H_

#include "logger_impl.h"
#include "logger_policy.h"

namespace logger {

template <                   //
    severity_type def,       //
    typename... log_polices> //
class Logger {

  public:
    Logger(log_polices &&... lp) noexcept
        : m_impl(std::make_unique<detail::LoggerImpl<def, log_polices...>>(
              std::forward<log_polices>(lp)...)) {}
    /**
     *
     */
    template <typename C, typename S, typename... Args>
    void debug(const C &tag, const S &format_str, Args &&... args) {
        if constexpr (def >= severity_type::debug)
            m_impl->print(detail::sdebug(), tag, format_str,
                          std::forward<Args>(args)...);
    }
    /**
     *
     */
    template <typename C, typename S, typename... Args>
    void info(const C &tag, const S &format_str, Args &&... args) {
        if constexpr (def >= severity_type::info)
            m_impl->print(detail::sinfo(), tag, format_str,
                          std::forward<Args>(args)...);
    }
    /**
     *
     */
    template <typename C, typename S, typename... Args>
    void error(const C &tag, const S &format_str, Args &&... args) {
        if constexpr (def >= severity_type::error)
            m_impl->print(detail::serror(), tag, format_str,
                          std::forward<Args>(args)...);
    }
    /**
     *
     */
    template <typename C, typename S, typename... Args>
    void warning(const C &tag, const S &format_str, Args &&... args) {
        if constexpr (def >= severity_type::warning)
            m_impl->print(detail::swarning(), tag, format_str,
                          std::forward<Args>(args)...);
    }
    /**
     *
     */
    iLogPolicy *policy(size_t i) const { return m_impl->policy(i); }
    /**
     *
     */
    template <typename C> void setLogSeverity(const C &tag, severity_type sev) {
        m_impl->setLogSeverity(tag, sev);
    }

  private:
    std::unique_ptr<detail::LoggerImpl<def, log_polices...>> m_impl;
};
} // namespace logger

#endif /* COMPONENTS_LOGGER_LOGGER_H_ */
