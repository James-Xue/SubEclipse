#pragma once

#include "subeclipse/capture.h"

#include <vector>

namespace subeclipse
{

/*
 * 文本检测结果。
 * rect 坐标系与输入 Frame 一致（ROI 局部坐标）。
 */
struct TextDetection
{
    RoiRect rect{};
    float score = 0.0F;
};

/*
 * 文本检测器抽象接口。
 * threshold 由上层配置传入，用于控制检测严格度。
 */
class ITextDetector
{
  public:
    virtual ~ITextDetector() = default;
    virtual std::vector<TextDetection> detect(const Frame &frame, float threshold) = 0;
};

/*
 * 简版文本检测器（启发式）。
 * 目标：不引入 OCR 依赖，先打通检测与遮挡流程。
 */
class SimpleTextDetector final : public ITextDetector
{
  public:
    std::vector<TextDetection> detect(const Frame &frame, float threshold) override;
};

} // namespace subeclipse
