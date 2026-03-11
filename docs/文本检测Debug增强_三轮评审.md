# 文本检测 Debug 增强三轮评审

日期：2026-03-12  
评审人：GitHub Copilot（GPT-5.3-Codex）

## 变更范围
- `src/vision/simple_text_detector.cpp`

## 第1轮评审：日志覆盖与行为一致性
### 目标
确认日志覆盖关键检测阶段，且不改变检测算法行为。

### 检查结果
- 已覆盖输入、参数映射、row 统计、band 提取、列投影、候选过滤原因、最终输出计数。
- 关键判定逻辑未变：
  - `gx + gy >= grad_threshold`
  - `density >= row_density_threshold`
  - `width < max(12, frame.width/20)` 过滤
  - `score >= threshold * 0.5` 保留
- 新增日志以统计和关键事件为主，没有逐像素刷屏。

### 结论
通过。

## 第2轮评审：构建与诊断
### 验证命令
- `cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug`
- `cmake --build build-debug -j`

### 结果
- 构建成功：`subeclipse_core` 与 `subeclipse` 均成功。
- 目标文件诊断：`src/vision/simple_text_detector.cpp` 无错误。

### 结论
通过。

## 第3轮评审：可维护性与调试可用性
### 目标
确认日志信息足以定位“为何检测不到文本”，且结构可维护。

### 检查结果
- 日志前缀分层清晰：`[Detector][Input/Params/Row/Band/Col/Candidate/Output]`。
- 候选 reject 原因明确（`width_too_small`、`score_too_small`），便于针对性调参。
- 统计项可直接用于诊断：
  - 行命中分布（min/max/mean/hit_rows）
  - band 数量与每段密度
  - 每段 active_segments 与 col_threshold
- 代码结构保持函数职责清晰，未引入接口破坏。

### 结论
通过。

## 最终结论
本次“文本检测 Debug 增强”已完成，并通过三轮评审。当前版本可用于你边调试边反馈日志，我可据日志继续精确定位漏检原因。