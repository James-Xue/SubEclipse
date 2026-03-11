# VSCode Release 一键编译/运行配置三轮评审

日期：2026-03-11  
评审人：GitHub Copilot（GPT-5.3-Codex）

## 第1轮：结构与需求匹配评审
### 目标
确认配置是否满足“VSCode 一键编译 Release + F5 一键运行 Release”。

### 检查结果
- `tasks.json` 已包含：`Configure Release`、`Build Release`、`Rebuild Release`。
- `Build Release` 通过 `dependsOn` 串联 `Configure Release`，顺序正确。
- `launch.json` 含 `cppdbg + gdb` 启动项，`program` 指向 `${workspaceFolder}/build/subeclipse`。
- `preLaunchTask` 正确绑定 `Build Release`，满足 F5 前自动编译。

### 结论
通过。

## 第2轮：构建与运行验证评审
### 目标
验证任务命令和 F5 目标路径是否真实可用。

### 验证
- 执行：`cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j`。
- 结果：构建成功，生成 `build/subeclipse`。
- 执行：`timeout 3s ./build/subeclipse`。
- 结果：程序可启动并输出日志（3 秒超时退出属于预期）。

### 结论
通过。

## 第3轮：配置质量与兼容性评审
### 目标
检查配置文件合法性、可维护性与基础兼容性。

### 检查结果
- `tasks.json` 与 `launch.json` 语法诊断均为无错误。
- `cwd` 统一为 `${workspaceFolder}`，路径稳定。
- 不依赖 CMake Tools 扩展命令，跨环境通用性较好。
- 配置保持最小化，便于后续扩展（如 Debug/RelWithDebInfo）。

### 结论
通过。

## 最终结论
子代理交付的 VSCode 配置满足当前“初步验证”目标，可直接使用：
- `Ctrl+Shift+B`：执行默认 `Build Release`
- `F5`：先编译 Release，再运行 `build/subeclipse`
