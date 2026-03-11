#include "subeclipse/config.h"

#include <fstream>
#include <regex>
#include <sstream>

namespace subeclipse
{

    namespace
    {
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

        void parse_string_field(const std::string &json, const std::string &key, std::string &out_value)
        {
            const std::regex pattern("\\\"" + key + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
            std::smatch match;
            if (std::regex_search(json, match, pattern) && match.size() > 1)
            {
                out_value = match[1].str();
            }
        }

        void parse_int_field(const std::string &json, const std::string &key, int &out_value)
        {
            const std::regex pattern("\\\"" + key + "\\\"\\s*:\\s*(-?[0-9]+)");
            std::smatch match;
            if (std::regex_search(json, match, pattern) && match.size() > 1)
            {
                out_value = std::stoi(match[1].str());
            }
        }
    } // namespace

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

        if (config.window_width <= 0)
        {
            config.window_width = 800;
        }
        if (config.window_height <= 0)
        {
            config.window_height = 450;
        }
        if (config.window_show_ms < 0)
        {
            config.window_show_ms = 1200;
        }

        return config;
    }

} // namespace subeclipse
