#include "subeclipse/config.h"
#include "subeclipse/detector.h"
#include "subeclipse/logger.h"
#include "subeclipse/overlay_window.h"
#include "subeclipse/pipeline.h"
#include "subeclipse/roi.h"

#include <X11/keysym.h>

#include <memory>
#include <thread>
#include <utility>
#include <vector>

namespace
{
/*
 * 进入编辑模式：关闭穿透并请求重绘。
 * 该函数仅封装状态切换，不引入额外语义。
 */
void enter_edit_mode(subeclipse::OverlayWindow &overlay, bool &running, bool &need_redraw)
{
    running = false;
    overlay.set_click_through(false);
    need_redraw = true;
}

/*
 * 尝试进入运行模式。
 * ROI 为空时保持原状态并记录告警，返回 false。
 */
bool enter_running_mode(subeclipse::OverlayWindow &overlay,
    const subeclipse::RoiEditor &roi,
    subeclipse::CaptureVisionPipeline &pipeline,
    const subeclipse::AppConfig &config,
    bool &running,
    bool &need_redraw)
{
    if (!roi.has_roi())
    {
        subeclipse::Logger::warn("Cannot start: ROI is empty, please draw ROI first");
        return false;
    }

    if (!pipeline.start(roi.rect(), config.capture_fps, config.detect_threshold))
    {
        subeclipse::Logger::error("Cannot start: pipeline init failed");
        return false;
    }

    running = true;
    overlay.set_click_through(true);
    need_redraw = true;
    return true;
}

/*
 * 处理鼠标事件。
 * 仅在编辑模式生效，运行模式保持 click-through 语义。
 */
void handle_mouse_event(const subeclipse::OverlayEvent &event,
    bool running,
    subeclipse::OverlayWindow &overlay,
    subeclipse::RoiEditor &roi,
    bool &need_redraw)
{
    if (running)
    {
        return;
    }

    switch (event.type)
    {
    case subeclipse::OverlayEvent::Type::MousePress:
        roi.on_mouse_press(event.x, event.y, overlay.width(), overlay.height());
        need_redraw = true;
        break;
    case subeclipse::OverlayEvent::Type::MouseMove:
        roi.on_mouse_move(event.x, event.y, overlay.width(), overlay.height());
        need_redraw = true;
        break;
    case subeclipse::OverlayEvent::Type::MouseRelease:
        roi.on_mouse_release(overlay.width(), overlay.height());
        need_redraw = true;
        break;
    default:
        break;
    }
}

/*
 * 处理按键事件。
 * 保持现有快捷键语义：Q/Esc 退出，Space 切换运行态，R 重选 ROI。
 */
void handle_key_event(const subeclipse::OverlayEvent &event,
    subeclipse::OverlayWindow &overlay,
    subeclipse::RoiEditor &roi,
    subeclipse::CaptureVisionPipeline &pipeline,
    const subeclipse::AppConfig &config,
    std::vector<subeclipse::RoiRect> &mask_boxes,
    bool &running,
    bool &need_redraw,
    bool &should_exit)
{
    if (event.keysym == XK_q || event.keysym == XK_Q || event.keysym == XK_Escape)
    {
        should_exit = true;
        return;
    }

    if (event.keysym == XK_space)
    {
        if (running)
        {
            pipeline.stop();
            mask_boxes.clear();
            enter_edit_mode(overlay, running, need_redraw);
            subeclipse::Logger::info("Paused: edit mode");
        }
        else if (enter_running_mode(overlay, roi, pipeline, config, running, need_redraw))
        {
            subeclipse::Logger::info("Running: click-through on");
        }
        return;
    }

    if (event.keysym == XK_r || event.keysym == XK_R)
    {
        pipeline.stop();
        mask_boxes.clear();
        roi.clear();
        enter_edit_mode(overlay, running, need_redraw);
        subeclipse::Logger::info("ROI cleared: draw new ROI");
    }
}

/*
 * 单轮事件泵：消费当前队列中的全部事件并完成状态更新。
 * 将事件分发从 main() 中提炼，降低主循环分支深度。
 */
void pump_events_once(subeclipse::OverlayWindow &overlay,
    subeclipse::RoiEditor &roi,
    subeclipse::CaptureVisionPipeline &pipeline,
    const subeclipse::AppConfig &config,
    std::vector<subeclipse::RoiRect> &mask_boxes,
    bool &running,
    bool &need_redraw,
    bool &should_exit)
{
    subeclipse::OverlayEvent event;
    while (overlay.poll_event(event))
    {
        switch (event.type)
        {
        case subeclipse::OverlayEvent::Type::Close:
            should_exit = true;
            break;
        case subeclipse::OverlayEvent::Type::Redraw:
            need_redraw = true;
            break;
        case subeclipse::OverlayEvent::Type::MousePress:
        case subeclipse::OverlayEvent::Type::MouseMove:
        case subeclipse::OverlayEvent::Type::MouseRelease:
            handle_mouse_event(event, running, overlay, roi, need_redraw);
            break;
        case subeclipse::OverlayEvent::Type::Key:
            handle_key_event(event, overlay, roi, pipeline, config, mask_boxes, running, need_redraw, should_exit);
            break;
        case subeclipse::OverlayEvent::Type::Empty:
            break;
        }
    }
}
} // namespace

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
    CaptureVisionPipeline pipeline(std::make_unique<X11Capture>(), std::make_unique<SimpleTextDetector>());

    /*
     * 运行态含义：
     * - running = false：编辑模式（可鼠标编辑 ROI，窗口不穿透）；
     * - running = true：运行模式（禁用 ROI 编辑，窗口 click-through）。
     */
    bool running = false;
    /* 延迟重绘标志：只在状态/输入变化时重绘，避免无效刷新。 */
    bool need_redraw = true;
    bool should_exit = false;
    std::vector<RoiRect> mask_boxes;
    /* 抗闪烁：连续空检测帧计数，达到阈值后才清空遮挡框。 */
    int empty_detection_frames = 0;
    constexpr int kMaxEmptyDetectionFrames = 3;

    if (config.mask_style != "black_box")
    {
        Logger::warn("Unsupported mask_style, fallback to black_box");
    }

    enter_edit_mode(overlay, running, need_redraw);
    Logger::info("Controls: Space start/pause, R reselect ROI, Q/Esc quit");

    while (!should_exit)
    {
        /*
         * 单轮事件泵：先快速消费队列，再按需重绘一次。
         * 这样可减少重复绘制，且让状态切换在同一循环内收敛。
         */
        pump_events_once(overlay, roi, pipeline, config, mask_boxes, running, need_redraw, should_exit);

        if (running)
        {
            pipeline.update_roi(roi.rect());

            const auto latest = pipeline.latest_detection();
            if (latest.has_value())
            {
                const RoiRect roi_rect = roi.rect();
                std::vector<RoiRect> latest_boxes;
                latest_boxes.reserve(latest->detections.size());

                for (const TextDetection &det : latest->detections)
                {
                    if (det.rect.width <= 0 || det.rect.height <= 0)
                    {
                        continue;
                    }

                    /* 检测框是 ROI 局部坐标，这里平移回 overlay 窗口坐标后再绘制。 */
                    latest_boxes.push_back(RoiRect{
                        roi_rect.x + det.rect.x,
                        roi_rect.y + det.rect.y,
                        det.rect.width,
                        det.rect.height,
                    });
                }

                if (!latest_boxes.empty())
                {
                    /* 非空结果立即生效，并清零空帧计数。 */
                    mask_boxes = std::move(latest_boxes);
                    empty_detection_frames = 0;
                    need_redraw = true;
                }
                else
                {
                    /* 抗闪烁：单帧漏检不清空，连续 N 帧空结果后再清空。 */
                    ++empty_detection_frames;
                    if (empty_detection_frames >= kMaxEmptyDetectionFrames)
                    {
                        if (!mask_boxes.empty())
                        {
                            mask_boxes.clear();
                            need_redraw = true;
                        }
                    }
                }
            }
        }
        else
        {
            /* 切回编辑态时重置，避免下次运行时沿用旧计数。 */
            empty_detection_frames = 0;
        }

        if (need_redraw)
        {
            overlay.draw(roi, mask_boxes);
            need_redraw = false;
        }

        /*
         * 轻量 sleep 降低空闲 CPU 占用。
         * 边界：间隔过大将导致拖拽反馈变钝，8ms 在流畅度与负载间折中。
         */
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

    pipeline.stop();
    Logger::info("Exiting SubEclipse overlay");
    return 0;
}
