#pragma once

namespace subeclipse
{

    struct RoiRect
    {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
    };

    class RoiEditor
    {
    public:
        void clear();
        void on_mouse_press(int x, int y, int canvas_width, int canvas_height);
        void on_mouse_move(int x, int y, int canvas_width, int canvas_height);
        void on_mouse_release(int canvas_width, int canvas_height);

        bool has_roi() const;
        RoiRect rect() const;
        RoiRect handle_rect() const;
        bool point_in_roi(int x, int y) const;
        bool point_in_handle(int x, int y) const;

    private:
        enum class DragMode
        {
            None,
            Creating,
            Moving,
            Resizing,
        };

        static constexpr int kMinSize = 20;
        static constexpr int kHandleSize = 12;

        void clamp_to_canvas(int canvas_width, int canvas_height);
        static int clamp_int(int value, int min_value, int max_value);

        bool has_roi_ = false;
        DragMode drag_mode_ = DragMode::None;
        RoiRect roi_{};
        RoiRect drag_origin_{};
        int anchor_x_ = 0;
        int anchor_y_ = 0;
        int press_x_ = 0;
        int press_y_ = 0;
    };

} // namespace subeclipse
