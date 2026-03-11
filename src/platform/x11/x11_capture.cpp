#include "subeclipse/capture.h"

#include "subeclipse/logger.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

#include <algorithm>
#include <chrono>
#include <limits>
#include <sys/ipc.h>
#include <sys/shm.h>

namespace subeclipse
{

namespace
{
constexpr std::int64_t kReconnectIntervalMs = 1000;
constexpr std::int64_t kWarnIntervalMs = 1000;
} // namespace

struct X11Capture::Impl
{
    Display *display = nullptr;
    int screen = 0;
    Window root = 0;

    RoiRect roi{};
    bool roi_set = false;

    bool shm_available = false;
    XImage *shm_image = nullptr;
    XShmSegmentInfo shm_info{};
    int shm_width = 0;
    int shm_height = 0;

    std::int64_t last_reconnect_ms = 0;
    std::int64_t last_warn_ms = 0;
};

X11Capture::X11Capture() : impl_(new Impl())
{
}

X11Capture::~X11Capture()
{
    disconnect_display();
    delete impl_;
    impl_ = nullptr;
}

void X11Capture::set_roi(const RoiRect &roi)
{
    impl_->roi = roi;
    impl_->roi_set = true;
}

bool X11Capture::grab(Frame &frame)
{
    if (!ensure_connected())
    {
        return false;
    }

    if (!impl_->roi_set)
    {
        warn_throttled("X11Capture: ROI 未设置，跳过抓屏");
        return false;
    }

    RoiRect roi = impl_->roi;
    if (!validate_and_clamp_roi(roi))
    {
        return false;
    }

    if (impl_->shm_available && grab_with_shm(roi, frame))
    {
        frame.ts_ms = now_ms();
        return true;
    }

    if (grab_with_xgetimage(roi, frame))
    {
        frame.ts_ms = now_ms();
        return true;
    }

    warn_throttled("X11Capture: XShm 与 XGetImage 均失败，稍后重试并尝试重连");
    disconnect_display();
    return false;
}

bool X11Capture::ensure_connected()
{
    if (impl_->display != nullptr)
    {
        return true;
    }

    const std::int64_t now = now_ms();
    if (now - impl_->last_reconnect_ms < kReconnectIntervalMs)
    {
        return false;
    }
    impl_->last_reconnect_ms = now;

    impl_->display = XOpenDisplay(nullptr);
    if (impl_->display == nullptr)
    {
        warn_throttled("X11Capture: 无法连接 X11 Display，将在 1 秒后重试");
        return false;
    }

    impl_->screen = DefaultScreen(impl_->display);
    impl_->root = RootWindow(impl_->display, impl_->screen);

    int major = 0;
    int minor = 0;
    Bool pixmaps = False;
    impl_->shm_available = XShmQueryVersion(impl_->display, &major, &minor, &pixmaps) != 0;

    if (impl_->shm_available)
    {
        Logger::info("X11Capture: 已启用 XShm 优先抓屏");
    }
    else
    {
        Logger::warn("X11Capture: XShm 不可用，将使用 XGetImage 抓屏");
    }
    return true;
}

void X11Capture::disconnect_display()
{
    release_shm_image();

    if (impl_->display != nullptr)
    {
        XCloseDisplay(impl_->display);
        impl_->display = nullptr;
    }

    impl_->screen = 0;
    impl_->root = 0;
    impl_->shm_available = false;
}

bool X11Capture::validate_and_clamp_roi(RoiRect &roi)
{
    if (roi.width <= 0 || roi.height <= 0)
    {
        warn_throttled("X11Capture: ROI 非法（宽高必须大于 0）");
        return false;
    }

    const int screen_width = DisplayWidth(impl_->display, impl_->screen);
    const int screen_height = DisplayHeight(impl_->display, impl_->screen);
    if (screen_width <= 0 || screen_height <= 0)
    {
        warn_throttled("X11Capture: 无法获取屏幕尺寸");
        return false;
    }

    int x1 = roi.x;
    int y1 = roi.y;
    int x2 = roi.x + roi.width;
    int y2 = roi.y + roi.height;

    x1 = std::clamp(x1, 0, screen_width);
    y1 = std::clamp(y1, 0, screen_height);
    x2 = std::clamp(x2, 0, screen_width);
    y2 = std::clamp(y2, 0, screen_height);

    if (x2 <= x1 || y2 <= y1)
    {
        warn_throttled("X11Capture: ROI 越界或为空，跳过抓屏");
        return false;
    }

    roi.x = x1;
    roi.y = y1;
    roi.width = x2 - x1;
    roi.height = y2 - y1;
    return true;
}

bool X11Capture::grab_with_shm(const RoiRect &roi, Frame &frame)
{
    if (!ensure_shm_image(roi.width, roi.height))
    {
        return false;
    }

    if (XShmGetImage(impl_->display, impl_->root, impl_->shm_image, roi.x, roi.y, AllPlanes) == 0)
    {
        warn_throttled("X11Capture: XShmGetImage 失败，回退到 XGetImage");
        return false;
    }

    return copy_ximage_to_bgra(static_cast<void *>(impl_->shm_image), frame);
}

bool X11Capture::grab_with_xgetimage(const RoiRect &roi, Frame &frame)
{
    XImage *image = XGetImage(impl_->display,
        impl_->root,
        roi.x,
        roi.y,
        static_cast<unsigned int>(roi.width),
        static_cast<unsigned int>(roi.height),
        AllPlanes,
        ZPixmap);

    if (image == nullptr)
    {
        warn_throttled("X11Capture: XGetImage 失败");
        return false;
    }

    const bool ok = copy_ximage_to_bgra(static_cast<void *>(image), frame);
    XDestroyImage(image);
    return ok;
}

bool X11Capture::ensure_shm_image(int width, int height)
{
    if (!impl_->shm_available)
    {
        return false;
    }

    if (impl_->shm_image != nullptr && impl_->shm_width == width && impl_->shm_height == height)
    {
        return true;
    }

    release_shm_image();

    XImage *image = XShmCreateImage(impl_->display,
        DefaultVisual(impl_->display, impl_->screen),
        static_cast<unsigned int>(DefaultDepth(impl_->display, impl_->screen)),
        ZPixmap,
        nullptr,
        &impl_->shm_info,
        static_cast<unsigned int>(width),
        static_cast<unsigned int>(height));

    if (image == nullptr)
    {
        warn_throttled("X11Capture: XShmCreateImage 失败，禁用 XShm");
        impl_->shm_available = false;
        return false;
    }

    const std::size_t bytes = static_cast<std::size_t>(image->bytes_per_line) * static_cast<std::size_t>(image->height);
    impl_->shm_info.shmid = shmget(IPC_PRIVATE, bytes, IPC_CREAT | 0600);
    if (impl_->shm_info.shmid < 0)
    {
        XDestroyImage(image);
        warn_throttled("X11Capture: shmget 失败，禁用 XShm");
        impl_->shm_available = false;
        return false;
    }

    impl_->shm_info.shmaddr = static_cast<char *>(shmat(impl_->shm_info.shmid, nullptr, 0));
    if (impl_->shm_info.shmaddr == reinterpret_cast<char *>(-1))
    {
        shmctl(impl_->shm_info.shmid, IPC_RMID, nullptr);
        impl_->shm_info.shmaddr = nullptr;
        impl_->shm_info.shmid = -1;
        XDestroyImage(image);
        warn_throttled("X11Capture: shmat 失败，禁用 XShm");
        impl_->shm_available = false;
        return false;
    }

    image->data = impl_->shm_info.shmaddr;
    impl_->shm_info.readOnly = False;
    if (XShmAttach(impl_->display, &impl_->shm_info) == 0)
    {
        shmdt(impl_->shm_info.shmaddr);
        shmctl(impl_->shm_info.shmid, IPC_RMID, nullptr);
        impl_->shm_info.shmaddr = nullptr;
        impl_->shm_info.shmid = -1;
        XDestroyImage(image);
        warn_throttled("X11Capture: XShmAttach 失败，禁用 XShm");
        impl_->shm_available = false;
        return false;
    }

    XSync(impl_->display, False);
    shmctl(impl_->shm_info.shmid, IPC_RMID, nullptr);

    impl_->shm_image = image;
    impl_->shm_width = width;
    impl_->shm_height = height;
    return true;
}

void X11Capture::release_shm_image()
{
    if (impl_->shm_image == nullptr)
    {
        return;
    }

    if (impl_->display != nullptr)
    {
        XShmDetach(impl_->display, &impl_->shm_info);
        XSync(impl_->display, False);
    }

    XDestroyImage(impl_->shm_image);
    impl_->shm_image = nullptr;

    if (impl_->shm_info.shmaddr != nullptr && impl_->shm_info.shmaddr != reinterpret_cast<char *>(-1))
    {
        shmdt(impl_->shm_info.shmaddr);
    }

    impl_->shm_info = XShmSegmentInfo{};
    impl_->shm_width = 0;
    impl_->shm_height = 0;
}

std::int64_t X11Capture::now_ms()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::uint8_t X11Capture::channel_from_mask(unsigned long pixel, unsigned long mask)
{
    if (mask == 0UL)
    {
        return 0;
    }

    unsigned long shifted_mask = mask;
    unsigned int shift = 0;
    while ((shifted_mask & 1UL) == 0UL)
    {
        shifted_mask >>= 1U;
        ++shift;
    }

    unsigned int bits = 0;
    while ((shifted_mask & 1UL) != 0UL)
    {
        shifted_mask >>= 1U;
        ++bits;
    }

    if (bits == 0)
    {
        return 0;
    }

    const unsigned long raw = (pixel & mask) >> shift;
    const unsigned long max_value =
        (bits >= sizeof(unsigned long) * 8) ? std::numeric_limits<unsigned long>::max() : ((1UL << bits) - 1UL);
    if (max_value == 0UL)
    {
        return 0;
    }

    return static_cast<std::uint8_t>((raw * 255UL) / max_value);
}

bool X11Capture::copy_ximage_to_bgra(void *ximage_ptr, Frame &frame)
{
    XImage *image = static_cast<XImage *>(ximage_ptr);
    if (image == nullptr || image->width <= 0 || image->height <= 0)
    {
        return false;
    }

    frame.width = image->width;
    frame.height = image->height;
    frame.bgra.resize(static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height) * 4U);

    for (int y = 0; y < image->height; ++y)
    {
        for (int x = 0; x < image->width; ++x)
        {
            const unsigned long pixel = XGetPixel(image, x, y);
            const std::uint8_t r = channel_from_mask(pixel, image->red_mask);
            const std::uint8_t g = channel_from_mask(pixel, image->green_mask);
            const std::uint8_t b = channel_from_mask(pixel, image->blue_mask);

            const std::size_t idx =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(image->width) + static_cast<std::size_t>(x)) *
                4U;
            frame.bgra[idx + 0] = b;
            frame.bgra[idx + 1] = g;
            frame.bgra[idx + 2] = r;
            frame.bgra[idx + 3] = 255U;
        }
    }

    return true;
}

void X11Capture::warn_throttled(const char *message)
{
    const std::int64_t now = now_ms();
    if (now - impl_->last_warn_ms < kWarnIntervalMs)
    {
        return;
    }
    impl_->last_warn_ms = now;
    Logger::warn(message);
}

} // namespace subeclipse
