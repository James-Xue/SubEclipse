#include "subeclipse/config.h"
#include "subeclipse/logger.h"
#include "subeclipse/overlay_window.h"
#include "subeclipse/roi.h"

#include <X11/keysym.h>

#include <chrono>
#include <thread>

/*
 * 主程序职责：
 * 1) 初始化配置与日志；
 * 2) 创建覆盖窗口；
 * 3) 在主循环中处理事件、维护运行/编辑状态并触发重绘。
 *
 * 设计取向：将“事件采集”和“业务状态切换”集中在单线程主循环，
 * 保证交互可预测，降低并发同步复杂度。
 */
int main()
{
    using namespace subeclipse;

    /* 配置加载失败时会自动使用默认值，确保可启动。 */
    const AppConfig config = load_config("configs/default.json");
    Logger::set_level(Logger::parse_level(config.log_level));

    /* 覆盖窗口创建失败属于致命错误，直接退出。 */
    OverlayWindow overlay;
    if (!overlay.create(config.window_width, config.window_height, "SubEclipse Overlay"))
    {
        return 1;
    }

    RoiEditor roi;
    /*
     * 运行态含义：
     * - running = false：编辑模式（可鼠标编辑 ROI，窗口不穿透）；
     * - running = true：运行模式（禁用 ROI 编辑，窗口 click-through）。
     */
    bool running = false;
    /* 延迟重绘标志：只在状态/输入变化时重绘，避免无效刷新。 */
    bool need_redraw = true;
    bool should_exit = false;

    /*
     * 进入编辑模式。
     * 为什么显式封装：避免在多个分支重复写状态切换细节，
     * 降低遗漏 click-through 状态同步的风险。
     */
    auto enter_edit_mode = [&]()
    {
        running = false;
        overlay.set_click_through(false);
        need_redraw = true;
    };

    /*
     * 进入运行模式。
     * 关键边界：ROI 为空时禁止进入，避免“运行中但无目标区域”的无效状态。
     */
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
        /*
         * 先快速消费当前队列中的全部事件，再按需重绘一次。
         * 这样做可减少重复绘制，且让状态切换在同一循环内收敛。
         */
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
                /* 仅编辑模式处理鼠标，运行模式保持穿透语义。 */
                if (!running)
                {
                    roi.on_mouse_press(event.x, event.y, overlay.width(), overlay.height());
                    need_redraw = true;
                }
                break;
            case OverlayEvent::Type::MouseMove:
                /* ROI 交互是“按下后移动”的连续过程，移动事件决定动态反馈。 */
                if (!running)
                {
                    roi.on_mouse_move(event.x, event.y, overlay.width(), overlay.height());
                    need_redraw = true;
                }
                break;
            case OverlayEvent::Type::MouseRelease:
                /* 释放时收敛 ROI 几何并结束拖拽态。 */
                if (!running)
                {
                    roi.on_mouse_release(overlay.width(), overlay.height());
                    need_redraw = true;
                }
                break;
            case OverlayEvent::Type::Key:
                /* 快捷键在任意模式都生效，保障可随时退出/切换。 */
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
                    /* 强制重选 ROI：清空后回编辑态，防止旧 ROI 残留。 */
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

        /*
         * 轻量 sleep 降低空闲 CPU 占用。
         * 边界：间隔过大将导致拖拽反馈变钝，8ms 在流畅度与负载间折中。
         */
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

    Logger::info("Exiting SubEclipse overlay");
    return 0;
}
