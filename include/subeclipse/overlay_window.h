#pragma once

#include "subeclipse/roi.h"

#include <X11/Xlib.h>

namespace subeclipse
{

    /*
     * 覆盖层事件抽象。
     * 目的：将 X11 原生事件压平为主循环可直接消费的统一类型，
     * 避免业务层依赖过多 XEvent 细节。
     */
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

    /*
     * X11 覆盖窗口封装。
     * 核心职责：
     * 1) 创建顶层可绘制窗口并维护图形上下文；
     * 2) 轮询并转换输入/窗口事件；
     * 3) 通过输入形状（ShapeInput）切换 click-through。
     *
     * click-through 技术点：
     * - 开启时将输入区域设为空区域，鼠标事件穿透到底层窗口；
     * - 关闭时恢复整窗输入区域，允许 ROI 编辑交互。
     */
    class OverlayWindow
    {
    public:
        OverlayWindow() = default;
        ~OverlayWindow();

        /*
         * 创建覆盖窗口。
         * 关键边界：宽高必须为正；X11 连接失败时返回 false。
         */
        bool create(int width, int height, const char *title);
        /* 释放窗口与 X11 资源，可重复调用。 */
        void destroy();

        /* 当前实例是否处于可用状态。 */
        bool is_valid() const;
        /* 当前窗口宽度（会随配置事件更新）。 */
        int width() const;
        /* 当前窗口高度（会随配置事件更新）。 */
        int height() const;

        /*
         * 非阻塞轮询单个事件。
         * 返回 true 表示拿到一条可消费事件；false 表示当前无事件。
         */
        bool poll_event(OverlayEvent &event);
        /* 根据 ROI 状态重绘覆盖层。 */
        void draw(const RoiEditor &roi);

        /* 切换 click-through 模式。 */
        void set_click_through(bool enabled);
        /* 查询 click-through 当前状态。 */
        bool click_through_enabled() const;

    private:
        /* 按名称解析颜色像素值，失败时回退白色。 */
        unsigned long color_pixel(const char *name) const;
        /* 注册全局快捷键（空格/R/Q/Esc）。 */
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
