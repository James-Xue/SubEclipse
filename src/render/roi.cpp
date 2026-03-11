#include "subeclipse/roi.h"

#include <algorithm>

namespace subeclipse
{

    /* 清空 ROI 并重置交互状态。 */
    void RoiEditor::clear()
    {
        has_roi_ = false;
        drag_mode_ = DragMode::None;
        roi_ = RoiRect{};
    }

    /*
     * 鼠标按下时决定拖拽语义。
     * 优先命中控制柄，其次 ROI 内移动，最后才是新建。
     * 这是 ROI 交互一致性的关键：同一位置永远触发同一类操作。
     */
    void RoiEditor::on_mouse_press(int x, int y, int canvas_width, int canvas_height)
    {
        press_x_ = x;
        press_y_ = y;

        if (has_roi_ && point_in_handle(x, y))
        {
            drag_mode_ = DragMode::Resizing;
            drag_origin_ = roi_;
            return;
        }

        if (has_roi_ && point_in_roi(x, y))
        {
            drag_mode_ = DragMode::Moving;
            drag_origin_ = roi_;
            return;
        }

        drag_mode_ = DragMode::Creating;
        has_roi_ = true;
        anchor_x_ = x;
        anchor_y_ = y;
        roi_ = RoiRect{x, y, 0, 0};
        clamp_to_canvas(canvas_width, canvas_height);
    }

    /*
     * 鼠标移动时按状态机更新 ROI。
     * - Creating：由锚点与当前点构造矩形；
     * - Moving：基于按下点位移整体平移；
     * - Resizing：固定左上角，仅扩展宽高。
     */
    void RoiEditor::on_mouse_move(int x, int y, int canvas_width, int canvas_height)
    {
        switch (drag_mode_)
        {
        case DragMode::Creating:
        {
            roi_.x = std::min(anchor_x_, x);
            roi_.y = std::min(anchor_y_, y);
            roi_.width = std::abs(x - anchor_x_);
            roi_.height = std::abs(y - anchor_y_);
            break;
        }
        case DragMode::Moving:
        {
            roi_.x = drag_origin_.x + (x - press_x_);
            roi_.y = drag_origin_.y + (y - press_y_);
            break;
        }
        case DragMode::Resizing:
        {
            roi_.x = drag_origin_.x;
            roi_.y = drag_origin_.y;
            roi_.width = drag_origin_.width + (x - press_x_);
            roi_.height = drag_origin_.height + (y - press_y_);
            break;
        }
        case DragMode::None:
            return;
        }

        /* 每次交互更新后立刻裁剪，避免越界传播到后续计算。 */
        clamp_to_canvas(canvas_width, canvas_height);
    }

    /*
     * 鼠标释放时收敛结果。
     * 关键边界：先裁剪，再应用最小尺寸，再次裁剪。
     * 原因：最小尺寸扩张后可能再次触碰画布边界。
     */
    void RoiEditor::on_mouse_release(int canvas_width, int canvas_height)
    {
        if (drag_mode_ == DragMode::None)
        {
            return;
        }

        clamp_to_canvas(canvas_width, canvas_height);
        if (roi_.width < kMinSize)
        {
            roi_.width = kMinSize;
        }
        if (roi_.height < kMinSize)
        {
            roi_.height = kMinSize;
        }
        clamp_to_canvas(canvas_width, canvas_height);
        drag_mode_ = DragMode::None;
    }

    /* 返回是否已有 ROI。 */
    bool RoiEditor::has_roi() const
    {
        return has_roi_;
    }

    /* 返回当前 ROI。 */
    RoiRect RoiEditor::rect() const
    {
        return roi_;
    }

    /*
     * 计算右下角控制柄。
     * 约定控制柄贴右下角，便于用户直觉地“向外拖拽放大”。
     */
    RoiRect RoiEditor::handle_rect() const
    {
        return RoiRect{
            roi_.x + roi_.width - kHandleSize,
            roi_.y + roi_.height - kHandleSize,
            kHandleSize,
            kHandleSize};
    }

    /* ROI 区域命中测试。 */
    bool RoiEditor::point_in_roi(int x, int y) const
    {
        if (!has_roi_)
        {
            return false;
        }
        /* 使用闭区间命中，减少边界像素“点不中”的体验问题。 */
        return x >= roi_.x && y >= roi_.y && x <= roi_.x + roi_.width && y <= roi_.y + roi_.height;
    }

    /* 控制柄命中测试。 */
    bool RoiEditor::point_in_handle(int x, int y) const
    {
        if (!has_roi_)
        {
            return false;
        }
        const RoiRect handle = handle_rect();
        return x >= handle.x && y >= handle.y && x <= handle.x + handle.width && y <= handle.y + handle.height;
    }

    /*
     * 将 ROI 约束到画布内。
     * 处理顺序：
     * 1) 负宽高归零；
     * 2) 左上角坐标钳制；
     * 3) 若右/下越界，收缩宽高。
     */
    void RoiEditor::clamp_to_canvas(int canvas_width, int canvas_height)
    {
        if (!has_roi_)
        {
            return;
        }

        if (roi_.width < 0)
        {
            roi_.width = 0;
        }
        if (roi_.height < 0)
        {
            roi_.height = 0;
        }

        roi_.x = clamp_int(roi_.x, 0, std::max(0, canvas_width - 1));
        roi_.y = clamp_int(roi_.y, 0, std::max(0, canvas_height - 1));

        if (roi_.x + roi_.width > canvas_width)
        {
            roi_.width = std::max(0, canvas_width - roi_.x);
        }
        if (roi_.y + roi_.height > canvas_height)
        {
            roi_.height = std::max(0, canvas_height - roi_.y);
        }
    }

    /* 整数钳制辅助函数。 */
    int RoiEditor::clamp_int(int value, int min_value, int max_value)
    {
        return std::max(min_value, std::min(max_value, value));
    }

} // namespace subeclipse
