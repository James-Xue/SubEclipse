#include "subeclipse/config.h"

#include <fstream>
#include <regex>
#include <sstream>

namespace subeclipse
{

namespace
{
/*
 * 读取整个文件内容。
 * 设计选择：失败时返回空串而非抛异常，
 * 由上层统一走“默认配置兜底”路径，保持启动流程稳定。
 */
std::string read_all(const std::string &path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        return {};
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

/*
 * 解析 JSON 风格字符串字段。
 * 说明：当前实现使用轻量正则而非完整 JSON 库，
 * 目的是减少依赖并满足现有简单配置场景。
 */
void parse_string_field(const std::string &json, const std::string &key, std::string &out_value)
{
    const std::regex pattern("\\\"" + key + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
    std::smatch match;
    if (std::regex_search(json, match, pattern) && match.size() > 1)
    {
        out_value = match[1].str();
    }
}

/*
 * 解析 JSON 风格整数字段。
 * 关键边界：支持负号，后续由 load_config 做业务合法性校验。
 */
void parse_int_field(const std::string &json, const std::string &key, int &out_value)
{
    const std::regex pattern("\\\"" + key + "\\\"\\s*:\\s*(-?[0-9]+)");
    std::smatch match;
    if (std::regex_search(json, match, pattern) && match.size() > 1)
    {
        out_value = std::stoi(match[1].str());
    }
}

/*
 * 解析 JSON 风格浮点字段。
 * 兼容整数与小数表示。
 */
void parse_float_field(const std::string &json, const std::string &key, float &out_value)
{
    const std::regex pattern("\\\"" + key + "\\\"\\s*:\\s*(-?[0-9]+(?:\\.[0-9]+)?)");
    std::smatch match;
    if (std::regex_search(json, match, pattern) && match.size() > 1)
    {
        out_value = std::stof(match[1].str());
    }
}
} // namespace

/*
 * 加载配置并做最小合法性修正。
 *
 * 设计原则：
 * 1) 先以结构体默认值初始化，任何解析失败都不会使程序不可启动；
 * 2) 对尺寸和时长做边界收敛，避免后续窗口创建参数非法；
 * 3) 仅做必要修正，不在这里引入业务策略。
 */
AppConfig load_config(const std::string &path)
{
    AppConfig config;
    const std::string text = read_all(path);
    if (text.empty())
    {
        return config;
    }

    parse_string_field(text, "log_level", config.log_level);
    parse_int_field(text, "window_width", config.window_width);
    parse_int_field(text, "window_height", config.window_height);
    parse_int_field(text, "window_show_ms", config.window_show_ms);
    parse_float_field(text, "detect_threshold", config.detect_threshold);
    parse_int_field(text, "capture_fps", config.capture_fps);
    parse_int_field(text, "empty_detection_clear_frames", config.empty_detection_clear_frames);
    parse_string_field(text, "mask_style", config.mask_style);

    /* 宽高必须为正值，否则回落到可见且常用的默认分辨率。 */
    if (config.window_width <= 0)
    {
        config.window_width = 800;
    }
    if (config.window_height <= 0)
    {
        config.window_height = 450;
    }
    /* 时间参数允许 0（表示无等待），但不允许负值。 */
    if (config.window_show_ms < 0)
    {
        config.window_show_ms = 1200;
    }
    if (config.detect_threshold < 0.0F)
    {
        config.detect_threshold = 0.0F;
    }
    if (config.detect_threshold > 1.0F)
    {
        config.detect_threshold = 1.0F;
    }
    if (config.capture_fps <= 0)
    {
        config.capture_fps = 5;
    }
    if (config.empty_detection_clear_frames <= 0)
    {
        config.empty_detection_clear_frames = 3;
    }
    if (config.mask_style.empty())
    {
        config.mask_style = "black_box";
    }

    return config;
}

} // namespace subeclipse
