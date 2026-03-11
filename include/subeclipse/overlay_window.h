#pragma once

#include "subeclipse/roi.h"

#include <X11/Xlib.h>

namespace subeclipse
{

    struct OverlayEvent
    {
        enum class Type
        {
            Empty,
            Close,
            Key,
            MousePress,
            MouseRelease,
            MouseMove,
            Redraw,
        };

        Type type = Type::Empty;
        KeySym keysym = 0;
        int x = 0;
        int y = 0;
    };

    class OverlayWindow
    {
    public:
        OverlayWindow() = default;
        ~OverlayWindow();

        bool create(int width, int height, const char *title);
        void destroy();

        bool is_valid() const;
        int width() const;
        int height() const;

        bool poll_event(OverlayEvent &event);
        void draw(const RoiEditor &roi);

        void set_click_through(bool enabled);
        bool click_through_enabled() const;

    private:
        unsigned long color_pixel(const char *name) const;
        void grab_hotkeys();

        Display *display_ = nullptr;
        int screen_ = 0;
        Window window_ = 0;
        Colormap colormap_ = 0;
        GC gc_red_ = 0;
        GC gc_white_ = 0;
        GC gc_bg_ = 0;
        Atom wm_delete_ = 0;
        int width_ = 0;
        int height_ = 0;
        bool click_through_ = false;
    };

} // namespace subeclipse
