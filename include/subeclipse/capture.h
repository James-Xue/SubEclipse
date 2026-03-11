#pragma once

#include "subeclipse/roi.h"

#include <cstdint>
#include <vector>

namespace subeclipse
{

/*
 * 抓屏帧数据。
 * 约定：像素采用 BGRA8（每像素 4 字节，A 固定 255）。
 */
struct Frame
{
    std::int64_t ts_ms = 0;
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> bgra;
};

/*
 * 抓屏抽象接口。
 * 设计目标：隔离平台细节，便于后续扩展 Wayland/其他平台实现。
 */
class IScreenCapture
{
  public:
    virtual ~IScreenCapture() = default;

    /* 设置当前抓取 ROI（屏幕坐标）。 */
    virtual void set_roi(const RoiRect &roi) = 0;
    /* 抓取一帧，成功返回 true。 */
    virtual bool grab(Frame &frame) = 0;
};

/*
 * X11 抓屏实现。
 * 策略：优先 XShmGetImage，失败自动回退 XGetImage。
 */
class X11Capture final : public IScreenCapture
{
  public:
    X11Capture();
    ~X11Capture() override;

    void set_roi(const RoiRect &roi) override;
    bool grab(Frame &frame) override;

  private:
    bool ensure_connected();
    void disconnect_display();

    bool validate_and_clamp_roi(RoiRect &roi);
    bool grab_with_shm(const RoiRect &roi, Frame &frame);
    bool grab_with_xgetimage(const RoiRect &roi, Frame &frame);

    bool ensure_shm_image(int width, int height);
    void release_shm_image();

    static std::int64_t now_ms();
    static std::uint8_t channel_from_mask(unsigned long pixel, unsigned long mask);
    static bool copy_ximage_to_bgra(void *ximage_ptr, Frame &frame);

    void warn_throttled(const char *message);

  private:
    struct Impl;
    Impl *impl_ = nullptr;
};

} // namespace subeclipse
