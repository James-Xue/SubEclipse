#include "subeclipse/roi.h"

#include <algorithm>

namespace subeclipse
{

    void RoiEditor::clear()
    {
        has_roi_ = false;
        drag_mode_ = DragMode::None;
        roi_ = RoiRect{};
    }

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

        clamp_to_canvas(canvas_width, canvas_height);
    }

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

    bool RoiEditor::has_roi() const
    {
        return has_roi_;
    }

    RoiRect RoiEditor::rect() const
    {
        return roi_;
    }

    RoiRect RoiEditor::handle_rect() const
    {
        return RoiRect{
            roi_.x + roi_.width - kHandleSize,
            roi_.y + roi_.height - kHandleSize,
            kHandleSize,
            kHandleSize};
    }

    bool RoiEditor::point_in_roi(int x, int y) const
    {
        if (!has_roi_)
        {
            return false;
        }
        return x >= roi_.x && y >= roi_.y && x <= roi_.x + roi_.width && y <= roi_.y + roi_.height;
    }

    bool RoiEditor::point_in_handle(int x, int y) const
    {
        if (!has_roi_)
        {
            return false;
        }
        const RoiRect handle = handle_rect();
        return x >= handle.x && y >= handle.y && x <= handle.x + handle.width && y <= handle.y + handle.height;
    }

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

    int RoiEditor::clamp_int(int value, int min_value, int max_value)
    {
        return std::max(min_value, std::min(max_value, value));
    }

} // namespace subeclipse
