#include "subeclipse/config.h"
#include "subeclipse/logger.h"
#include "subeclipse/overlay_window.h"
#include "subeclipse/roi.h"

#include <X11/keysym.h>

#include <chrono>
#include <thread>

int main()
{
    using namespace subeclipse;

    const AppConfig config = load_config("configs/default.json");
    Logger::set_level(Logger::parse_level(config.log_level));

    OverlayWindow overlay;
    if (!overlay.create(config.window_width, config.window_height, "SubEclipse Overlay"))
    {
        return 1;
    }

    RoiEditor roi;
    bool running = false;
    bool need_redraw = true;
    bool should_exit = false;

    auto enter_edit_mode = [&]()
    {
        running = false;
        overlay.set_click_through(false);
        need_redraw = true;
    };

    auto enter_running_mode = [&]()
    {
        if (!roi.has_roi())
        {
            Logger::warn("Cannot start: ROI is empty, please draw ROI first");
            return;
        }
        running = true;
        overlay.set_click_through(true);
        need_redraw = true;
    };

    enter_edit_mode();
    Logger::info("Controls: Space start/pause, R reselect ROI, Q/Esc quit");

    while (!should_exit)
    {
        OverlayEvent event;
        while (overlay.poll_event(event))
        {
            switch (event.type)
            {
            case OverlayEvent::Type::Close:
                should_exit = true;
                break;
            case OverlayEvent::Type::Redraw:
                need_redraw = true;
                break;
            case OverlayEvent::Type::MousePress:
                if (!running)
                {
                    roi.on_mouse_press(event.x, event.y, overlay.width(), overlay.height());
                    need_redraw = true;
                }
                break;
            case OverlayEvent::Type::MouseMove:
                if (!running)
                {
                    roi.on_mouse_move(event.x, event.y, overlay.width(), overlay.height());
                    need_redraw = true;
                }
                break;
            case OverlayEvent::Type::MouseRelease:
                if (!running)
                {
                    roi.on_mouse_release(overlay.width(), overlay.height());
                    need_redraw = true;
                }
                break;
            case OverlayEvent::Type::Key:
                if (event.keysym == XK_q || event.keysym == XK_Q || event.keysym == XK_Escape)
                {
                    should_exit = true;
                }
                else if (event.keysym == XK_space)
                {
                    if (running)
                    {
                        enter_edit_mode();
                        Logger::info("Paused: edit mode");
                    }
                    else
                    {
                        enter_running_mode();
                        if (running)
                        {
                            Logger::info("Running: click-through on");
                        }
                    }
                }
                else if (event.keysym == XK_r || event.keysym == XK_R)
                {
                    roi.clear();
                    enter_edit_mode();
                    Logger::info("ROI cleared: draw new ROI");
                }
                break;
            case OverlayEvent::Type::Empty:
                break;
            }
        }

        if (need_redraw)
        {
            overlay.draw(roi);
            need_redraw = false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

    Logger::info("Exiting SubEclipse overlay");
    return 0;
}
