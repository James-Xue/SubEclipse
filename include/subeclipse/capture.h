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
    /*
     * 构造函数。
     * 仅完成实现体分配，不立即建立 X11 连接，遵循“按需连接”策略，
     * 以降低程序启动时对图形环境可用性的强依赖。
     */
    X11Capture();
    /*
     * 析构函数。
     * 负责释放 Display、XShm 图像与共享内存等底层资源，
     * 保证对象离开作用域后不会遗留系统级句柄。
     */
    ~X11Capture() override;

    /*
     * 更新抓取 ROI。
     * 该操作仅更新内部快照，不进行即时合法性校验；
     * 真正校验在 grab() 前进行，以便统一结合当前屏幕尺寸裁剪。
     */
    void set_roi(const RoiRect &roi) override;
    /*
     * 抓取一帧屏幕图像。
     * 执行顺序：确保连接 -> 校验/裁剪 ROI -> 优先 XShm -> 回退 XGetImage。
     * 成功时输出 BGRA 帧并写入时间戳；失败时返回 false。
     */
    bool grab(Frame &frame) override;

  private:
    /* 确保与 X11 服务器连接可用；包含失败重连节流。 */
    bool ensure_connected();
    /* 断开 Display 并释放关联资源，可重复调用。 */
    void disconnect_display();

    /* 基于当前屏幕尺寸验证并裁剪 ROI，确保抓取参数合法。 */
    bool validate_and_clamp_roi(RoiRect &roi);
    /* 使用 XShm 路径抓取一帧（高性能路径）。 */
    bool grab_with_shm(const RoiRect &roi, Frame &frame);
    /* 使用 XGetImage 路径抓取一帧（兼容回退路径）。 */
    bool grab_with_xgetimage(const RoiRect &roi, Frame &frame);

    /* 确保共享内存图像缓冲可用，尺寸变化时会重建。 */
    bool ensure_shm_image(int width, int height);
    /* 释放共享内存图像与段映射，处理所有清理分支。 */
    void release_shm_image();

    /* 获取毫秒级时间戳。 */
    static std::int64_t now_ms();
    /*
     * 从像素位掩码提取并归一化通道值。
     * 用于兼容不同 XImage 像素格式/掩码布局。
     */
    static std::uint8_t channel_from_mask(unsigned long pixel, unsigned long mask);
    /* 将任意 XImage 数据转换为统一 BGRA 缓冲。 */
    static bool copy_ximage_to_bgra(void *ximage_ptr, Frame &frame);

    /* 告警节流输出，避免失败场景刷屏。 */
    void warn_throttled(const char *message);

  private:
    struct Impl;
    Impl *impl_ = nullptr;
};

} // namespace subeclipse
