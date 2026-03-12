#pragma once

#include "subeclipse/capture.h"
#include "subeclipse/detector.h"
#include "subeclipse/roi.h"

#include <condition_variable>
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

namespace subeclipse
{

/*
 * 单生产者单消费者阻塞队列。
 * 用于 capture->vision 流水线的低开销解耦。
 */
template <typename T> class SpscBlockingQueue
{
  public:
    explicit SpscBlockingQueue(std::size_t capacity) : capacity_(capacity)
    {
    }

    bool push(T value)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        /* 队列满时阻塞；若收到 stop 信号则立刻放弃写入。 */
        not_full_cv_.wait(lock, [this]() { return stopped_ || queue_.size() < capacity_; });
        if (stopped_)
        {
            return false;
        }
        queue_.push(std::move(value));
        not_empty_cv_.notify_one();
        return true;
    }

    bool pop(T &value)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        /* 队列空时阻塞；stop 后允许快速退出消费者线程。 */
        not_empty_cv_.wait(lock, [this]() { return stopped_ || !queue_.empty(); });
        if (queue_.empty())
        {
            /* stopped 且无数据时返回 false，避免线程悬挂等待。 */
            return false;
        }
        value = std::move(queue_.front());
        queue_.pop();
        not_full_cv_.notify_one();
        return true;
    }

    void stop()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = true;
        not_empty_cv_.notify_all();
        not_full_cv_.notify_all();
    }

    void reset()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopped_ = false;
        /* 启动新一轮前清空残留帧，避免旧数据串入新会话。 */
        std::queue<T> empty;
        queue_.swap(empty);
        not_empty_cv_.notify_all();
        not_full_cv_.notify_all();
    }

  private:
    std::size_t capacity_ = 1;
    std::queue<T> queue_;
    bool stopped_ = false;
    std::mutex mutex_;
    std::condition_variable not_empty_cv_;
    std::condition_variable not_full_cv_;
};

/*
 * 管线输出结果：含 ROI 局部坐标检测框与抓屏时间戳。
 */
struct DetectionBatch
{
    std::int64_t frame_ts_ms = 0;
    std::vector<TextDetection> detections;
};

/*
 * 抓屏->检测基础管线。
 * - capture 线程按指定 FPS 产出 Frame；
 * - vision 线程执行 ITextDetector 并产出最新检测结果。
 */
class CaptureVisionPipeline
{
  public:
    CaptureVisionPipeline(std::unique_ptr<IScreenCapture> capture, std::unique_ptr<ITextDetector> detector);
    ~CaptureVisionPipeline();

    bool start(const RoiRect &roi, int capture_fps, float detect_threshold);
    void update_roi(const RoiRect &roi);
    void stop();

    bool running() const;
    std::optional<DetectionBatch> latest_detection() const;

  private:
    void capture_loop();
    void vision_loop();

  private:
    std::unique_ptr<IScreenCapture> capture_;
    std::unique_ptr<ITextDetector> detector_;

    SpscBlockingQueue<Frame> frame_queue_{2};

    mutable std::mutex config_mutex_;
    RoiRect roi_{};
    int capture_fps_ = 5;
    float detect_threshold_ = 0.6F;

    mutable std::mutex result_mutex_;
    std::optional<DetectionBatch> latest_detection_;

    std::thread capture_thread_;
    std::thread vision_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
};

} // namespace subeclipse
