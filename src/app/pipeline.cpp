#include "subeclipse/pipeline.h"

#include "subeclipse/logger.h"

#include <algorithm>
#include <chrono>
#include <utility>

namespace subeclipse
{

/*
 * 构造时接管 capture 与 detector 的独占所有权。
 * 约束：两者生命周期与管线实例一致，避免外部并发释放。
 */
CaptureVisionPipeline::CaptureVisionPipeline(std::unique_ptr<IScreenCapture> capture,
    std::unique_ptr<ITextDetector> detector)
    : capture_(std::move(capture)), detector_(std::move(detector))
{
}

/*
 * 析构时确保后台线程已停止并完成 join。
 * 这样可避免对象成员已销毁但线程仍访问成员导致未定义行为。
 */
CaptureVisionPipeline::~CaptureVisionPipeline()
{
    stop();
}

/*
 * 启动抓屏-检测双线程管线。
 * 关键步骤：
 * 1) 先 stop() 清理旧实例状态，保证 start 可重入；
 * 2) 在配置锁内写入 ROI/FPS/阈值，防止与运行线程竞争；
 * 3) 清空上一轮检测结果，避免新一轮启动后读到陈旧数据；
 * 4) 重置队列与停止标记，再启动两个工作线程。
 */
bool CaptureVisionPipeline::start(const RoiRect &roi, int capture_fps, float detect_threshold)
{
    stop();

    if (!capture_ || !detector_)
    {
        Logger::error("Pipeline start failed: capture or detector is null");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(config_mutex_);
        roi_ = roi;
        capture_fps_ = std::max(1, capture_fps);
        detect_threshold_ = std::clamp(detect_threshold, 0.0F, 1.0F);
    }

    {
        std::lock_guard<std::mutex> lock(result_mutex_);
        latest_detection_.reset();
    }

    frame_queue_.reset();
    stop_requested_.store(false);
    running_.store(true);

    /*
     * 线程模型：
     * - capture_thread_: 负责按节拍抓帧并推入队列；
     * - vision_thread_: 负责消费队列并生成最新检测结果。
     */
    capture_thread_ = std::thread(&CaptureVisionPipeline::capture_loop, this);
    vision_thread_ = std::thread(&CaptureVisionPipeline::vision_loop, this);

    Logger::info("CaptureVisionPipeline started");
    return true;
}

/*
 * 运行时热更新 ROI。
 * 仅修改共享配置快照，不打断现有线程，下一帧抓屏即生效。
 */
void CaptureVisionPipeline::update_roi(const RoiRect &roi)
{
    std::lock_guard<std::mutex> lock(config_mutex_);
    roi_ = roi;
}

/*
 * 停止管线并等待线程退出。
 * 关键停止序列：
 * 1) 设置 stop_requested_ 让循环尽快退出；
 * 2) frame_queue_.stop() 唤醒可能阻塞在 push/pop 的线程；
 * 3) join 两个线程，确保后续对象销毁安全。
 */
void CaptureVisionPipeline::stop()
{
    if (!running_.load())
    {
        return;
    }

    stop_requested_.store(true);
    frame_queue_.stop();

    if (capture_thread_.joinable())
    {
        capture_thread_.join();
    }
    if (vision_thread_.joinable())
    {
        vision_thread_.join();
    }

    running_.store(false);
    Logger::info("CaptureVisionPipeline stopped");
}

/* 原子读取运行态，供 UI/主循环做快速状态判断。 */
bool CaptureVisionPipeline::running() const
{
    return running_.load();
}

/*
 * 获取最新检测结果快照。
 * 返回 optional 的拷贝，调用方无需持锁即可安全使用。
 */
std::optional<DetectionBatch> CaptureVisionPipeline::latest_detection() const
{
    std::lock_guard<std::mutex> lock(result_mutex_);
    return latest_detection_;
}

/*
 * 抓屏线程主循环。
 * 每轮读取最新配置，执行抓屏并投递到有界队列。
 * 有界队列容量很小（当前为 2）用于抑制积压，优先保证“新鲜度”。
 */
void CaptureVisionPipeline::capture_loop()
{
    while (!stop_requested_.load())
    {
        RoiRect roi{};
        int fps = 5;
        {
            /* 原子地读取本轮配置快照，避免半更新状态。 */
            std::lock_guard<std::mutex> lock(config_mutex_);
            roi = roi_;
            fps = std::max(1, capture_fps_);
        }

        capture_->set_roi(roi);

        Frame frame;
        if (capture_->grab(frame))
        {
            /*
             * push 失败通常表示队列已进入 stop 状态，
             * 此时直接退出线程，避免无意义忙等。
             */
            if (!frame_queue_.push(std::move(frame)))
            {
                break;
            }
        }

        /*
         * 以固定间隔限速，降低抓屏端 CPU 占用并与目标 FPS 对齐。
         * 边界：fps 最小为 1，避免除零。
         */
        const int interval_ms = std::max(1, 1000 / fps);
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
}

/*
 * 检测线程主循环。
 * 从队列取帧后执行检测，并覆盖写入“最新结果”。
 * 设计取舍：只保留最新批次，牺牲历史以减少 UI 消费复杂度。
 */
void CaptureVisionPipeline::vision_loop()
{
    while (!stop_requested_.load())
    {
        Frame frame;
        /* pop 返回 false 表示队列停止或无可用数据，线程应结束。 */
        if (!frame_queue_.pop(frame))
        {
            break;
        }

        float threshold = 0.6F;
        {
            std::lock_guard<std::mutex> lock(config_mutex_);
            threshold = detect_threshold_;
        }

        DetectionBatch batch;
        batch.frame_ts_ms = frame.ts_ms;
        batch.detections = detector_->detect(frame, threshold);

        {
            /*
             * 结果区采用“整批替换”策略，
             * 避免调用方读取到半更新状态。
             */
            std::lock_guard<std::mutex> lock(result_mutex_);
            latest_detection_ = std::move(batch);
        }
    }
}

} // namespace subeclipse
