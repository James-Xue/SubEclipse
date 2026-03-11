# main 函数降圈复杂度三轮评审

日期：2026-03-11  
评审人：GitHub Copilot（GPT-5.3-Codex）

## 第1轮：结构与复杂度评审
### 目标
确认 `src/main.cpp` 中 `main()` 的圈复杂度是否明显降低，且重构方式可维护。

### 结论
- 通过。
- `main()` 已从“大型事件分发 + 业务状态切换”重构为“初始化 + 主循环骨架”。
- 主要分支被提炼到以下函数：
  - `enter_edit_mode(...)`
  - `enter_running_mode(...)`
  - `handle_mouse_event(...)`
  - `handle_key_event(...)`
  - `pump_events_once(...)`
- 主循环可读性提升，分支嵌套深度明显下降。

## 第2轮：构建与运行一致性评审
### 目标
确认重构后功能无回归。

### 验证
- 构建命令：
  - `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j`
- 结果：通过，`subeclipse` 生成成功。
- 运行烟测：
  - `timeout 3s ./build/subeclipse`
- 结果：程序正常启动并输出控制提示日志（超时退出属预期）。

### 结论
- 通过。

## 第3轮：行为一致性与可维护性评审
### 目标
确认重构不改变行为语义，且后续扩展成本降低。

### 检查点
- 快捷键语义保持不变：`Q/Esc` 退出，`Space` 切换运行/编辑，`R` 重选 ROI。
- 鼠标语义保持不变：仅编辑模式响应鼠标，运行模式保持 click-through。
- 状态变量语义未变：`running/need_redraw/should_exit` 一致。
- 事件泵语义保持：先消费队列再按需重绘。

### 结论
- 通过。

## 最终结论
本次重构在不改变外部行为的前提下有效降低了 `main()` 圈复杂度，代码可读性与可维护性提升，可作为后续功能扩展基线。
