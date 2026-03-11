#pragma once

#include <string>

namespace subeclipse
{

    /*
     * 日志等级按“严重程度”递增排列。
     * 这样设计的原因：运行时过滤日志时可以直接用整数比较，
     * 避免复杂条件判断，且对 Info/Warn/Error 三层需求已足够。
     */
    enum class LogLevel
    {
        Info = 0,
        Warn = 1,
        Error = 2,
    };

    /*
     * 轻量级静态日志器。
     * 设计目标：
     * 1) 不要求调用方持有对象，降低主流程接入成本；
     * 2) 在多线程场景下保证单条日志原子输出，避免行间穿插；
     * 3) 提供最小但稳定的日志级别控制能力。
     */
    class Logger
    {
    public:
        /*
         * 设置全局日志阈值。
         * 约定：只输出“级别 >= 当前阈值”的日志。
         */
        static void set_level(LogLevel level);

        /*
         * 将配置文本解析为日志等级。
         * 关键边界：未知字符串会回落到 Info，保证系统可继续启动。
         */
        static LogLevel parse_level(const std::string &text);

        /* 输出普通运行信息。 */
        static void info(const std::string &message);
        /* 输出可能异常但可恢复的信息。 */
        static void warn(const std::string &message);
        /* 输出错误信息。 */
        static void error(const std::string &message);

    private:
        /*
         * 统一日志出口，负责过滤、时间戳拼接、输出流选择。
         * 放在私有区是为了避免绕过标准格式直接写日志。
         */
        static void log(LogLevel level, const std::string &message);
    };

} // namespace subeclipse
