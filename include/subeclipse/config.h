#pragma once

#include <string>

namespace subeclipse
{

    struct AppConfig
    {
        std::string log_level = "info";
        int window_width = 800;
        int window_height = 450;
        int window_show_ms = 1200;
    };

    AppConfig load_config(const std::string &path);

} // namespace subeclipse
