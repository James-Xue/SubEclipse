#include "subeclipse/config.h"
#include "subeclipse/logger.h"

#include <X11/Xlib.h>

#include <chrono>
#include <thread>

int main()
{
    using namespace subeclipse;

    const AppConfig config = load_config("configs/default.json");
    Logger::set_level(Logger::parse_level(config.log_level));

    Display *display = XOpenDisplay(nullptr);
    if (display == nullptr)
    {
        Logger::error("Failed to open X11 display");
        return 1;
    }

    const int screen = DefaultScreen(display);
    Window window = XCreateSimpleWindow(
        display,
        RootWindow(display, screen),
        50,
        50,
        static_cast<unsigned int>(config.window_width),
        static_cast<unsigned int>(config.window_height),
        1,
        BlackPixel(display, screen),
        WhitePixel(display, screen));

    if (window == 0)
    {
        Logger::error("Failed to create X11 window");
        XCloseDisplay(display);
        return 2;
    }

    XStoreName(display, window, "SubEclipse");
    XSelectInput(display, window, ExposureMask | KeyPressMask | StructureNotifyMask);
    XMapWindow(display, window);
    XFlush(display);

    Logger::info("Window opened, entering minimal loop");

    const auto start = std::chrono::steady_clock::now();
    while (true)
    {
        while (XPending(display) > 0)
        {
            XEvent event;
            XNextEvent(display, &event);
            if (event.type == DestroyNotify)
            {
                Logger::warn("Window destroy event received");
                XCloseDisplay(display);
                return 0;
            }
        }

        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - start)
                                    .count();

        if (elapsed_ms >= config.window_show_ms)
        {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    XDestroyWindow(display, window);
    XCloseDisplay(display);

    Logger::info("Window closed, exiting normally");
    return 0;
}
