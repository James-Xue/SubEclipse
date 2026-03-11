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
        /*
         * 全局日志锁。
         * 目的：保证“级别判断 + 时间戳生成 + 输出”是同一临界区，
         * 避免多线程下日志内容交错。
         */
        std::mutex g_log_mutex;
        /* 当前日志阈值，默认输出全部级别。 */
        LogLevel g_current_level = LogLevel::Info;

        /* 将枚举转换为稳定文本，便于人读与日志检索。 */
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

        /*
         * 日志过滤规则：仅输出严重程度不低于阈值的消息。
         * 依赖 LogLevel 的整数有序性（Info < Warn < Error）。
         */
        bool should_log(LogLevel message_level)
        {
            return static_cast<int>(message_level) >= static_cast<int>(g_current_level);
        }

        /*
         * 生成本地时间戳。
         * 采用 localtime_r 而非 localtime，避免线程安全问题。
         */
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

    /* 设置全局日志阈值。 */
    void Logger::set_level(LogLevel level)
    {
        std::lock_guard<std::mutex> lock(g_log_mutex);
        g_current_level = level;
    }

    /*
     * 解析配置中的文本日志级别。
     * 关键边界：未知值回退到 Info，避免因拼写问题丢失日志。
     */
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

    /* 输出 Info 级别日志。 */
    void Logger::info(const std::string &message)
    {
        log(LogLevel::Info, message);
    }

    /* 输出 Warn 级别日志。 */
    void Logger::warn(const std::string &message)
    {
        log(LogLevel::Warn, message);
    }

    /* 输出 Error 级别日志。 */
    void Logger::error(const std::string &message)
    {
        log(LogLevel::Error, message);
    }

    /*
     * 实际日志输出函数。
     * 约定：Error 走 stderr，其他级别走 stdout，便于外部重定向与监控。
     */
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
