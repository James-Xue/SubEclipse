#pragma once

namespace subeclipse
{

    /*
     * ROI 轴对齐矩形（屏幕/窗口像素坐标系）。
     * 约定：x,y 为左上角，width,height 为非负长度。
     */
    struct RoiRect
    {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
    };

    /*
     * ROI 编辑器：处理鼠标交互并维护矩形状态。
     *
     * 交互模型采用“按下-移动-释放”的显式状态机：
     * - Creating：新建矩形；
     * - Moving：整体平移；
     * - Resizing：拖拽右下角控制柄缩放；
     * - None：空闲态。
     *
     * 这样设计的原因：
     * 1) 保持每次鼠标移动只执行一种明确语义，避免手势冲突；
     * 2) 便于在边界（窗口边缘、最小尺寸）处做统一收敛处理；
     * 3) 状态简单，可预测，方便后续维护。
     */
    class RoiEditor
    {
    public:
        /* 清空 ROI 并回到空闲态。 */
        void clear();

        /*
         * 鼠标按下入口：决定后续拖拽语义。
         * 判定优先级：控制柄 > ROI 内部 > 新建。
         * 关键点：该优先级避免在控制柄区域误触发移动。
         */
        void on_mouse_press(int x, int y, int canvas_width, int canvas_height);

        /*
         * 鼠标移动入口：根据当前 DragMode 更新 ROI。
         * 关键边界：每次更新后都会裁剪到画布范围。
         */
        void on_mouse_move(int x, int y, int canvas_width, int canvas_height);

        /*
         * 鼠标释放入口：收敛最终几何形态并退出拖拽态。
         * 关键边界：应用最小尺寸限制后再次裁剪，确保有效范围。
         */
        void on_mouse_release(int canvas_width, int canvas_height);

        /* 当前是否存在有效 ROI。 */
        bool has_roi() const;
        /* 返回当前 ROI。 */
        RoiRect rect() const;
        /* 返回右下角缩放控制柄矩形。 */
        RoiRect handle_rect() const;
        /* 命中检测：点是否在 ROI 内。 */
        bool point_in_roi(int x, int y) const;
        /* 命中检测：点是否在控制柄内。 */
        bool point_in_handle(int x, int y) const;

    private:
        /* ROI 编辑状态机。 */
        enum class DragMode
        {
            None,
            Creating,
            Moving,
            Resizing,
        };

        /* 最小 ROI 尺寸，防止生成不可用的小区域。 */
        static constexpr int kMinSize = 20;
        /* 控制柄边长，保证可点击性。 */
        static constexpr int kHandleSize = 12;

        /* 将 ROI 裁剪到画布内，避免绘制/命中越界。 */
        void clamp_to_canvas(int canvas_width, int canvas_height);
        /* 整数钳制工具函数。 */
        static int clamp_int(int value, int min_value, int max_value);

        /* ROI 是否已被创建。 */
        bool has_roi_ = false;
        /* 当前拖拽状态。 */
        DragMode drag_mode_ = DragMode::None;
        /* 当前 ROI。 */
        RoiRect roi_{};
        /* 拖拽起始时 ROI 快照，用于计算增量。 */
        RoiRect drag_origin_{};
        /* 新建模式锚点（按下位置）。 */
        int anchor_x_ = 0;
        int anchor_y_ = 0;
        /* 本次拖拽按下位置。 */
        int press_x_ = 0;
        int press_y_ = 0;
    };

} // namespace subeclipse
