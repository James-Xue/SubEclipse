#pragma once

#include <string>

namespace subeclipse
{

    /*
     * 应用配置结构。
     * 默认值即“可运行兜底值”：即便配置文件缺失或字段异常，
     * 程序仍能以合理窗口尺寸和显示时序启动。
     */
    struct AppConfig
    {
        /* 日志级别字符串（如 info/warn/error）。 */
        std::string log_level = "info";
        /* 主覆盖窗口宽度，必须为正值。 */
        int window_width = 800;
        /* 主覆盖窗口高度，必须为正值。 */
        int window_height = 450;
        /* 预留显示时长参数（毫秒），允许为 0，不允许负数。 */
        int window_show_ms = 1200;
    };

    /*
     * 从给定路径加载配置。
     * 设计原则：容错优先，缺失字段不报错，直接保留默认值。
     */
    AppConfig load_config(const std::string &path);

} // namespace subeclipse
