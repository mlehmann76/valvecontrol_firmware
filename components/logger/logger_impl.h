/*
 * logger.h
 *
 *  Created on: 12.10.2020
 *      Author: marco
 */

#ifndef COMPONENTS_LOGGER_LOGGER_IMPL_H_
#define COMPONENTS_LOGGER_LOGGER_IMPL_H_

#include <logger_def.h>

#include <iostream>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <memory>

namespace logger {

template<typename ... Args>
std::string string_format( const std::string& format, Args ... args )
{
    int size_s = ::snprintf( nullptr, 0, format.c_str(), args ... ) + 1; // Extra space for '\0'
    if( size_s <= 0 ){ return ""; /*throw std::runtime_error( "Error during formatting." ); */}
    auto size = static_cast<size_t>( size_s );
    auto buf = std::make_unique<char[]>( size );
    ::snprintf( buf.get(), size, format.c_str(), args ... );
    return std::string( buf.get(), buf.get() + size - 1 ); // We don't want the '\0' inside
}

struct iLogPolicy;
namespace detail {

template <               //
    severity_type def,   //
    class... log_polices //
    >                    //
class LoggerImpl {

    struct severity {
        constexpr severity(severity_type v = def) : value(v) {}
        severity_type value;
    };

    struct count {
        static const std::size_t value = sizeof...(log_polices);
    };

    using mapType = std::unordered_map<std::string, severity>;
    using arrayType = std::array<std::unique_ptr<iLogPolicy>, count::value>;

  public:
    LoggerImpl(log_polices &&... lp) noexcept
        : m_startTime(std::chrono::steady_clock::now()), m_index(0) {
        create(std::forward<log_polices>(lp)...);
        for (auto &l : m_policies)
            l->open();
    }

    ~LoggerImpl() {
        for (auto &l : m_policies)
            l->close();
    }

    iLogPolicy *policy(size_t i) const {
        return m_policies[i % count::value].get();
    }

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
            m_stream << string_format(format_str, args...);
            m_stream << "\n";
            print_impl(severity);
        }
    }

    template <typename C> void setLogSeverity(const C &tag, severity_type sev) {
        m_map[tag] = sev;
    }

  private:
    template <class T, class... Ts> void create(T p1, Ts &&... ts) {
        m_policies[m_index++] = std::make_unique<T>(std::forward<T>(p1));
        create(std::forward<Ts>(ts)...);
    }

    void create() {}

    template <class T> void print_impl(const T &severity) {
        for (auto &l : m_policies)
            l->write(severity.type, m_stream.str());
        m_stream.str("");
    }

    std::stringstream m_stream;
    arrayType m_policies;
    std::mutex m_mutex;
    std::chrono::steady_clock::time_point m_startTime;
    mapType m_map;
    size_t m_index;
};

/**
 * traits
 */
template <severity_type s, char *n> struct severity_trait {
    static constexpr severity_type type = s;
    static constexpr const char *name = n;
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
    severity_type::debug, detail::debug_string>;
using serror = detail::severity_trait< //
    severity_type::error, detail::error_string>;
using swarning = detail::severity_trait< //
    severity_type::warning, detail::warning_string>;
using sinfo = detail::severity_trait< //
    severity_type::info, detail::info_string>;

} // namespace detail

} // namespace logger

#endif /* COMPONENTS_LOGGER_LOGGER_IMPL_H_ */
