#include "subeclipse/overlay_window.h"

#include "subeclipse/logger.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xfixes.h>
#include <X11/keysym.h>

#include <cstring>

namespace subeclipse
{

    /* 析构时统一释放 X11 资源，避免泄漏。 */
    OverlayWindow::~OverlayWindow()
    {
        destroy();
    }

    /*
     * 创建 X11 覆盖窗口。
     * 关键设计点：
     * - 参数先做合法性检查，防止后续 XCreateWindow 失败难定位；
     * - 优先尝试 32-bit TrueColor 视觉，利于透明覆盖场景；
     * - 设置窗口类型与置顶状态，保证其作为 overlay 的行为预期。
     */
    bool OverlayWindow::create(int width, int height, const char *title)
    {
        if (width <= 0 || height <= 0)
        {
            Logger::error("Invalid overlay size");
            return false;
        }

        display_ = XOpenDisplay(nullptr);
        if (display_ == nullptr)
        {
            Logger::error("Failed to open X11 display");
            return false;
        }

        screen_ = DefaultScreen(display_);
        width_ = width;
        height_ = height;

        XVisualInfo visual_info{};
        if (!XMatchVisualInfo(display_, screen_, 32, TrueColor, &visual_info))
        {
            /* 回退到默认视觉，确保在能力较弱环境仍可创建窗口。 */
            visual_info.visual = DefaultVisual(display_, screen_);
            visual_info.depth = DefaultDepth(display_, screen_);
        }

        colormap_ = XCreateColormap(display_, RootWindow(display_, screen_), visual_info.visual, AllocNone);

        XSetWindowAttributes attrs{};
        attrs.colormap = colormap_;
        attrs.background_pixel = 0;
        attrs.border_pixel = 0;
        attrs.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                           StructureNotifyMask;

        window_ = XCreateWindow(display_,
                                RootWindow(display_, screen_),
                                0,
                                0,
                                static_cast<unsigned int>(width_),
                                static_cast<unsigned int>(height_),
                                0,
                                visual_info.depth,
                                InputOutput,
                                visual_info.visual,
                                CWColormap | CWBackPixel | CWBorderPixel | CWEventMask,
                                &attrs);

        if (window_ == 0)
        {
            Logger::error("Failed to create overlay window");
            destroy();
            return false;
        }

        XStoreName(display_, window_, title);

        /* 提示窗口管理器保持置顶。 */
        Atom wm_state = XInternAtom(display_, "_NET_WM_STATE", False);
        Atom wm_state_above = XInternAtom(display_, "_NET_WM_STATE_ABOVE", False);
        XChangeProperty(display_,
                        window_,
                        wm_state,
                        XA_ATOM,
                        32,
                        PropModeReplace,
                        reinterpret_cast<unsigned char *>(&wm_state_above),
                        1);

        /* 声明为 dock 类型，减少普通应用窗口管理策略干扰。 */
        Atom wm_type = XInternAtom(display_, "_NET_WM_WINDOW_TYPE", False);
        Atom wm_type_dock = XInternAtom(display_, "_NET_WM_WINDOW_TYPE_DOCK", False);
        XChangeProperty(display_,
                        window_,
                        wm_type,
                        XA_ATOM,
                        32,
                        PropModeReplace,
                        reinterpret_cast<unsigned char *>(&wm_type_dock),
                        1);

        wm_delete_ = XInternAtom(display_, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(display_, window_, &wm_delete_, 1);

        gc_red_ = XCreateGC(display_, window_, 0, nullptr);
        gc_white_ = XCreateGC(display_, window_, 0, nullptr);
        gc_bg_ = XCreateGC(display_, window_, 0, nullptr);

        XSetForeground(display_, gc_red_, color_pixel("red"));
        XSetForeground(display_, gc_white_, color_pixel("white"));
        XSetForeground(display_, gc_bg_, 0x00000000UL);

        XMapWindow(display_, window_);
        XFlush(display_);

        /* 绑定全局热键，确保 click-through 开启后仍可控制程序。 */
        grab_hotkeys();

        Logger::info("Overlay window created");
        return true;
    }

    /*
     * 销毁窗口及所有相关资源。
     * 关键点：允许重复调用（幂等），因此每一步都先判空/判零。
     */
    void OverlayWindow::destroy()
    {
        if (display_ == nullptr)
        {
            return;
        }

        /* 先取消热键抓取，避免遗留全局键盘占用。 */
        Window root = RootWindow(display_, screen_);
        const KeySym keysyms[] = {XK_space, XK_r, XK_R, XK_q, XK_Q, XK_Escape};
        for (KeySym keysym : keysyms)
        {
            const int keycode = XKeysymToKeycode(display_, keysym);
            if (keycode != 0)
            {
                XUngrabKey(display_, keycode, AnyModifier, root);
            }
        }

        if (gc_red_ != 0)
        {
            XFreeGC(display_, gc_red_);
            gc_red_ = 0;
        }
        if (gc_white_ != 0)
        {
            XFreeGC(display_, gc_white_);
            gc_white_ = 0;
        }
        if (gc_bg_ != 0)
        {
            XFreeGC(display_, gc_bg_);
            gc_bg_ = 0;
        }
        if (window_ != 0)
        {
            XDestroyWindow(display_, window_);
            window_ = 0;
        }
        if (colormap_ != 0)
        {
            XFreeColormap(display_, colormap_);
            colormap_ = 0;
        }

        XCloseDisplay(display_);
        display_ = nullptr;
    }

    /* 当前对象是否拥有可用窗口句柄。 */
    bool OverlayWindow::is_valid() const
    {
        return display_ != nullptr && window_ != 0;
    }

    /* 返回当前窗口宽度。 */
    int OverlayWindow::width() const
    {
        return width_;
    }

    /* 返回当前窗口高度。 */
    int OverlayWindow::height() const
    {
        return height_;
    }

    /*
     * 轮询并转换一个 X11 事件。
     * 设计目标：将复杂 XEvent 分支收敛为业务友好的 OverlayEvent。
     */
    bool OverlayWindow::poll_event(OverlayEvent &event)
    {
        event = OverlayEvent{};
        if (display_ == nullptr || XPending(display_) <= 0)
        {
            return false;
        }

        XEvent xevent{};
        XNextEvent(display_, &xevent);

        switch (xevent.type)
        {
        case Expose:
            /* Expose 表示窗口内容可能失效，触发重绘请求。 */
            event.type = OverlayEvent::Type::Redraw;
            return true;
        case MotionNotify:
            event.type = OverlayEvent::Type::MouseMove;
            event.x = xevent.xmotion.x;
            event.y = xevent.xmotion.y;
            return true;
        case ButtonPress:
            if (xevent.xbutton.button == Button1)
            {
                event.type = OverlayEvent::Type::MousePress;
                event.x = xevent.xbutton.x;
                event.y = xevent.xbutton.y;
                return true;
            }
            break;
        case ButtonRelease:
            if (xevent.xbutton.button == Button1)
            {
                event.type = OverlayEvent::Type::MouseRelease;
                event.x = xevent.xbutton.x;
                event.y = xevent.xbutton.y;
                return true;
            }
            break;
        case KeyPress:
            event.type = OverlayEvent::Type::Key;
            event.keysym = XLookupKeysym(&xevent.xkey, 0);
            return true;
        case ClientMessage:
            if (static_cast<Atom>(xevent.xclient.data.l[0]) == wm_delete_)
            {
                event.type = OverlayEvent::Type::Close;
                return true;
            }
            break;
        case ConfigureNotify:
            /* 窗口大小变化后更新缓存尺寸，保证 ROI 裁剪边界正确。 */
            width_ = xevent.xconfigure.width;
            height_ = xevent.xconfigure.height;
            event.type = OverlayEvent::Type::Redraw;
            return true;
        default:
            break;
        }

        return false;
    }

    /*
     * 执行一帧绘制。
     * 当前策略是“先清空再画 ROI”，保持输出确定性，避免残影。
     */
    void OverlayWindow::draw(const RoiEditor &roi)
    {
        if (!is_valid())
        {
            return;
        }

        XClearWindow(display_, window_);

        if (roi.has_roi())
        {
            const RoiRect rect = roi.rect();
            XDrawRectangle(display_,
                           window_,
                           gc_red_,
                           rect.x,
                           rect.y,
                           static_cast<unsigned int>(rect.width),
                           static_cast<unsigned int>(rect.height));

            const RoiRect handle = roi.handle_rect();
            XFillRectangle(display_,
                           window_,
                           gc_red_,
                           handle.x,
                           handle.y,
                           static_cast<unsigned int>(handle.width),
                           static_cast<unsigned int>(handle.height));
        }

        XFlush(display_);
    }

    /*
     * 切换 click-through（输入穿透）模式。
     *
     * 技术实现：
     * - enabled=true：将 ShapeInput 设置为空区域，窗口仅可见不可点击；
     * - enabled=false：恢复整窗输入区域，接收鼠标事件以编辑 ROI。
     *
     * 关键边界：仅在状态发生变化时执行，避免重复提交 X11 形状更新。
     */
    void OverlayWindow::set_click_through(bool enabled)
    {
        if (!is_valid() || click_through_ == enabled)
        {
            return;
        }

        click_through_ = enabled;

        if (enabled)
        {
            XserverRegion region = XFixesCreateRegion(display_, nullptr, 0);
            XFixesSetWindowShapeRegion(display_, window_, ShapeInput, 0, 0, region);
            XFixesDestroyRegion(display_, region);
            Logger::info("Click-through enabled (running)");
        }
        else
        {
            XRectangle full_rect{};
            full_rect.x = 0;
            full_rect.y = 0;
            full_rect.width = static_cast<unsigned short>(width_);
            full_rect.height = static_cast<unsigned short>(height_);
            XserverRegion region = XFixesCreateRegion(display_, &full_rect, 1);
            XFixesSetWindowShapeRegion(display_, window_, ShapeInput, 0, 0, region);
            XFixesDestroyRegion(display_, region);
            Logger::info("Click-through disabled (editing)");
        }

        XFlush(display_);
    }

    /* 返回 click-through 当前开关状态。 */
    bool OverlayWindow::click_through_enabled() const
    {
        return click_through_;
    }

    /*
     * 将颜色名解析为像素值。
     * 边界：解析失败回退白色，避免因主题/调色板异常导致不可见绘制。
     */
    unsigned long OverlayWindow::color_pixel(const char *name) const
    {
        if (display_ == nullptr)
        {
            return 0UL;
        }

        XColor color{};
        XColor exact{};
        if (XAllocNamedColor(display_, DefaultColormap(display_, screen_), name, &color, &exact) == 0)
        {
            return WhitePixel(display_, screen_);
        }
        return color.pixel;
    }

    /*
     * 抓取全局快捷键。
     * 同时注册无修饰、CapsLock、NumLock、两者叠加四种组合，
     * 解决锁定键状态变化导致快捷键失效的问题。
     */
    void OverlayWindow::grab_hotkeys()
    {
        Window root = RootWindow(display_, screen_);

        const KeySym keysyms[] = {XK_space, XK_r, XK_R, XK_q, XK_Q, XK_Escape};
        const unsigned int modifiers[] = {0U, LockMask, Mod2Mask, LockMask | Mod2Mask};

        for (KeySym keysym : keysyms)
        {
            const int keycode = XKeysymToKeycode(display_, keysym);
            if (keycode == 0)
            {
                continue;
            }

            for (unsigned int mod : modifiers)
            {
                XGrabKey(display_, keycode, mod, root, True, GrabModeAsync, GrabModeAsync);
            }
        }
    }

} // namespace subeclipse
