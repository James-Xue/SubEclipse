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
    /* 文本检测阈值，范围 [0,1]。 */
    float detect_threshold = 0.6F;
    /* 抓屏帧率（FPS），用于控制管线采样频率。 */
    int capture_fps = 5;
    /* 抗闪烁空帧阈值：连续空检测达到该帧数后清空遮挡。 */
    int empty_detection_clear_frames = 3;
    /* 遮挡样式（当前支持 black_box）。 */
    std::string mask_style = "black_box";
};

/*
 * 从给定路径加载配置。
 * 设计原则：容错优先，缺失字段不报错，直接保留默认值。
 */
AppConfig load_config(const std::string &path);

} // namespace subeclipse
