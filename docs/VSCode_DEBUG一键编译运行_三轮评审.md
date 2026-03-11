# VSCode DEBUG 一键编译与 F5 启动三轮评审

日期：2026-03-11  
评审人：GitHub Copilot（GPT-5.3-Codex）

## 第1轮评审：结构与需求匹配
- 检查 `tasks.json`：已包含 `Configure Debug`、`Build Debug`、`Rebuild Debug`。
- 检查 `launch.json`：已包含 `Run subeclipse (Debug)`，并置于第一个配置，`preLaunchTask` 为 `Build Debug`。
- 检查兼容性：保留了原 Release 任务与启动项。
- 结论：通过。

## 第2轮评审：真实链路验证
- 执行：`cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug && cmake --build build-debug -j`。
- 结果：构建成功，生成 `build-debug/subeclipse`。
- 执行：`timeout 3s ./build-debug/subeclipse`。
- 结果：程序正常启动输出日志（3 秒超时退出属预期烟测）。
- 结论：通过。

## 第3轮评审：可用性与默认行为
- 发现：`Ctrl+Shift+B` 默认仍指向 Release，不符合“DEBUG 一键编译”的直觉。
- 修正：将 `Build Debug` 设为默认构建任务（`isDefault: true`），`Build Release` 保留为可选构建任务。
- 复查：`tasks.json`、`launch.json` 均无诊断错误。
- 结论：通过。

## 最终结论
当前配置已满足：
- `Ctrl+Shift+B` 一键编译 Debug
- `F5` 一键启动 Debug（并自动预构建 Debug）
- Release 配置仍可用（手动选择任务/启动项）。
