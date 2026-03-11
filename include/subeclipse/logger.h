#pragma once

#include <string>

namespace subeclipse
{

    enum class LogLevel
    {
        Info = 0,
        Warn = 1,
        Error = 2,
    };

    class Logger
    {
    public:
        static void set_level(LogLevel level);
        static LogLevel parse_level(const std::string &text);

        static void info(const std::string &message);
        static void warn(const std::string &message);
        static void error(const std::string &message);

    private:
        static void log(LogLevel level, const std::string &message);
    };

} // namespace subeclipse
