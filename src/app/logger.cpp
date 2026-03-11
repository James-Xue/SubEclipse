#include "subeclipse/logger.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>

namespace subeclipse
{

    namespace
    {
        std::mutex g_log_mutex;
        LogLevel g_current_level = LogLevel::Info;

        const char *to_text(LogLevel level)
        {
            switch (level)
            {
            case LogLevel::Info:
                return "INFO";
            case LogLevel::Warn:
                return "WARN";
            case LogLevel::Error:
                return "ERROR";
            default:
                return "INFO";
            }
        }

        bool should_log(LogLevel message_level)
        {
            return static_cast<int>(message_level) >= static_cast<int>(g_current_level);
        }

        std::string now_timestamp()
        {
            const auto now = std::chrono::system_clock::now();
            const std::time_t tt = std::chrono::system_clock::to_time_t(now);

            std::tm local_tm{};
            localtime_r(&tt, &local_tm);

            std::ostringstream output;
            output << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
            return output.str();
        }
    } // namespace

    void Logger::set_level(LogLevel level)
    {
        std::lock_guard<std::mutex> lock(g_log_mutex);
        g_current_level = level;
    }

    LogLevel Logger::parse_level(const std::string &text)
    {
        if (text == "error" || text == "ERROR")
        {
            return LogLevel::Error;
        }
        if (text == "warn" || text == "WARN" || text == "warning" || text == "WARNING")
        {
            return LogLevel::Warn;
        }
        return LogLevel::Info;
    }

    void Logger::info(const std::string &message)
    {
        log(LogLevel::Info, message);
    }

    void Logger::warn(const std::string &message)
    {
        log(LogLevel::Warn, message);
    }

    void Logger::error(const std::string &message)
    {
        log(LogLevel::Error, message);
    }

    void Logger::log(LogLevel level, const std::string &message)
    {
        std::lock_guard<std::mutex> lock(g_log_mutex);
        if (!should_log(level))
        {
            return;
        }
        std::ostream &stream = (level == LogLevel::Error) ? std::cerr : std::cout;
        stream << "[" << now_timestamp() << "]"
               << " [" << to_text(level) << "] "
               << message << std::endl;
    }

} // namespace subeclipse
