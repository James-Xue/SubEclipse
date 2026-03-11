#include "subeclipse/detector.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace subeclipse
{

namespace
{
/*
 * BGRA 转灰度，范围 [0,255]。
 * 采用常见感知权重（BT.601 近似）以兼顾人眼亮度敏感度。
 */
inline std::uint8_t luma_from_bgra(const std::uint8_t *pixel)
{
    const float b = static_cast<float>(pixel[0]);
    const float g = static_cast<float>(pixel[1]);
    const float r = static_cast<float>(pixel[2]);
    const float y = 0.114F * b + 0.587F * g + 0.299F * r;
    return static_cast<std::uint8_t>(std::clamp(y, 0.0F, 255.0F));
}

/*
 * 整数钳制工具。
 * 用于构造检测框时收敛边界，避免负坐标或越界宽高。
 */
inline int clamp_int(int value, int min_v, int max_v)
{
    return std::max(min_v, std::min(max_v, value));
}
} // namespace

/*
 * 简版检测流程：
 * 1) 计算灰度图与水平梯度；
 * 2) 在行上统计“高对比像素密度”；
 * 3) 将高密度连续行聚类为文本带；
 * 4) 每个文本带内做列投影，生成一个或多个遮挡框。
 *
 * 该方案对“亮字/暗底”或“暗字/亮底”的高对比字幕都有一定鲁棒性，
 * 重点是打通 MVP 流程，不追求 OCR 级精度。
 */
std::vector<TextDetection> SimpleTextDetector::detect(const Frame &frame, float threshold)
{
    std::vector<TextDetection> out;

    /* 尺寸过小时梯度统计意义不大，直接返回空结果。 */
    if (frame.width <= 4 || frame.height <= 4)
    {
        return out;
    }
    const std::size_t expected_bytes =
        static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height) * 4U;
    if (frame.bgra.size() < expected_bytes)
    {
        return out;
    }

    /* 统一阈值范围，避免上层传入非法参数影响内部映射。 */
    const float t = std::clamp(threshold, 0.0F, 1.0F);

    /*
     * 先构建灰度图，后续所有梯度与投影都基于灰度进行，
     * 以降低彩色噪声对检测稳定性的影响。
     */
    std::vector<std::uint8_t> gray(static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height), 0);
    for (int y = 0; y < frame.height; ++y)
    {
        for (int x = 0; x < frame.width; ++x)
        {
            const std::size_t idx =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(x));
            gray[idx] = luma_from_bgra(&frame.bgra[idx * 4U]);
        }
    }

    /*
     * 将外部 threshold 映射到内部启发式参数：
     * - t 越大，检测越“保守”（更高对比要求、更低噪声容忍）；
     * - t 越小，检测越“敏感”（允许更多候选带/候选列）。
     */
    const int grad_threshold = 24 + static_cast<int>((1.0F - t) * 40.0F);
    const float row_density_threshold = 0.06F + (1.0F - t) * 0.08F;
    const int min_band_height = std::max(8, frame.height / 40);
    const int max_band_height = std::max(min_band_height + 1, frame.height / 3);

    /* 每一行命中的高梯度像素数量，用于后续行聚类。 */
    std::vector<int> row_hits(static_cast<std::size_t>(frame.height), 0);

    /*
     * 行方向扫描：同时考虑水平与垂直梯度（gx+gy），
     * 使算法对细笔画、描边与边缘变化更稳健。
     */
    for (int y = 1; y < frame.height - 1; ++y)
    {
        int hit_count = 0;
        for (int x = 1; x < frame.width - 1; ++x)
        {
            const std::size_t c =
                static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(x);
            const int gx = std::abs(static_cast<int>(gray[c + 1]) - static_cast<int>(gray[c - 1]));
            const int gy = std::abs(static_cast<int>(gray[c + static_cast<std::size_t>(frame.width)]) -
                                    static_cast<int>(gray[c - static_cast<std::size_t>(frame.width)]));
            if (gx + gy >= grad_threshold)
            {
                ++hit_count;
            }
        }
        row_hits[static_cast<std::size_t>(y)] = hit_count;
    }

    /*
     * 将“高密度行”合并为连续文本带（band）。
     * 通过最小/最大高度门限过滤零碎噪点与过高区域。
     */
    std::vector<std::pair<int, int>> bands;
    int band_start = -1;
    for (int y = 0; y < frame.height; ++y)
    {
        const float density = static_cast<float>(row_hits[static_cast<std::size_t>(y)]) /
                              static_cast<float>(std::max(1, frame.width - 2));

        if (density >= row_density_threshold)
        {
            if (band_start < 0)
            {
                band_start = y;
            }
        }
        else if (band_start >= 0)
        {
            const int h = y - band_start;
            if (h >= min_band_height && h <= max_band_height)
            {
                bands.emplace_back(band_start, y - 1);
            }
            band_start = -1;
        }
    }
    /* 尾段收口：处理最后一段以高密度行结束的情况。 */
    if (band_start >= 0)
    {
        const int h = frame.height - band_start;
        if (h >= min_band_height && h <= max_band_height)
        {
            bands.emplace_back(band_start, frame.height - 1);
        }
    }

    /*
     * 在每个 band 内做列投影：
     * 1) 统计每列高梯度命中；
     * 2) 将连续活跃列组成候选块；
     * 3) 对候选块扩边并评分，满足阈值后输出检测框。
     */
    for (const auto &band : bands)
    {
        const int y0 = band.first;
        const int y1 = band.second;
        const int band_h = y1 - y0 + 1;

        std::vector<int> col_hits(static_cast<std::size_t>(frame.width), 0);
        for (int y = y0; y <= y1; ++y)
        {
            for (int x = 1; x < frame.width - 1; ++x)
            {
                const std::size_t c =
                    static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) + static_cast<std::size_t>(x);
                const int gx = std::abs(static_cast<int>(gray[c + 1]) - static_cast<int>(gray[c - 1]));
                if (gx >= grad_threshold)
                {
                    ++col_hits[static_cast<std::size_t>(x)];
                }
            }
        }

        /* 列活跃阈值随 band 高度自适应，避免固定阈值在不同字号下失效。 */
        const int col_threshold = std::max(2, static_cast<int>(static_cast<float>(band_h) * 0.22F));
        int x_start = -1;
        for (int x = 0; x < frame.width; ++x)
        {
            const bool active = col_hits[static_cast<std::size_t>(x)] >= col_threshold;
            if (active)
            {
                if (x_start < 0)
                {
                    x_start = x;
                }
            }
            else if (x_start >= 0)
            {
                const int w = x - x_start;
                if (w >= std::max(12, frame.width / 20))
                {
                    /*
                     * 适度扩边覆盖描边/阴影，避免遮挡框过紧漏字。
                     * clamp_int 保证扩边后仍在图像内。
                     */
                    const int pad_x = 4;
                    const int pad_y = 3;
                    const int rx = clamp_int(x_start - pad_x, 0, frame.width - 1);
                    const int ry = clamp_int(y0 - pad_y, 0, frame.height - 1);
                    const int rw = clamp_int(w + pad_x * 2, 1, frame.width - rx);
                    const int rh = clamp_int(band_h + pad_y * 2, 1, frame.height - ry);

                    const float raw_score =
                        static_cast<float>(w * band_h) / static_cast<float>(std::max(1, frame.width * frame.height));
                    const float score = std::clamp(0.45F + raw_score * 8.0F, 0.0F, 1.0F);

                    /*
                     * MVP 期采用相对宽松的二次阈值（t*0.5）保留候选，
                     * 后续可在上层增加时序滤波进一步去抖。
                     */
                    if (score >= t * 0.5F)
                    {
                        out.push_back(TextDetection{RoiRect{rx, ry, rw, rh}, score});
                    }
                }
                x_start = -1;
            }
        }
        /* 与 band 收口同理：处理持续到最右侧的活跃列段。 */
        if (x_start >= 0)
        {
            const int w = frame.width - x_start;
            if (w >= std::max(12, frame.width / 20))
            {
                const int pad_x = 4;
                const int pad_y = 3;
                const int rx = clamp_int(x_start - pad_x, 0, frame.width - 1);
                const int ry = clamp_int(y0 - pad_y, 0, frame.height - 1);
                const int rw = clamp_int(w + pad_x * 2, 1, frame.width - rx);
                const int rh = clamp_int(band_h + pad_y * 2, 1, frame.height - ry);
                const float raw_score =
                    static_cast<float>(w * band_h) / static_cast<float>(std::max(1, frame.width * frame.height));
                const float score = std::clamp(0.45F + raw_score * 8.0F, 0.0F, 1.0F);
                if (score >= t * 0.5F)
                {
                    out.push_back(TextDetection{RoiRect{rx, ry, rw, rh}, score});
                }
            }
        }
    }

    /* 上限保护：避免异常场景输出过多框导致渲染/上层处理抖动。 */
    if (out.size() > 20)
    {
        out.resize(20);
    }
    return out;
}

} // namespace subeclipse
