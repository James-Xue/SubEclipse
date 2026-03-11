# SubEclipse
A fast, system-level screen overlay tool written in modern C++, designed for seamless subtitle masking with zero rendering overhead.

## Minimal Week-1 Skeleton (Linux/X11)

### Prerequisites
- CMake 3.16+
- C++20 compiler (GCC/Clang)
- X11 development libraries (e.g. `libx11-dev`)

### Build
```bash
cmake -S . -B build
cmake --build build -j
```

### Run
```bash
./build/subeclipse
```

The program opens a minimal X11 window and exits automatically after `window_show_ms` milliseconds.

## Week-2: ROI 交互与 Overlay 基础

当前版本已实现透明 Overlay 与 ROI 编辑基础能力（Linux/X11，无重型 GUI 框架）：

- 左键拖拽新建 ROI
- 左键拖拽 ROI 内部可移动
- 拖拽 ROI 右下角手柄可缩放
- `R`：清空并重选 ROI（进入编辑态）
- `Space`：运行/暂停切换
	- 运行态：启用 click-through（Overlay 不阻挡鼠标）
	- 暂停/编辑态：禁用 click-through（可继续鼠标编辑 ROI）
- `Q` 或 `Esc`：退出

说明：
- ROI 会以红色边框和右下角红色手柄显示在 Overlay 上。
- 当前 Week-2 仅实现交互与窗口层，不接入检测逻辑。

### Config
Configuration is loaded from `configs/default.json` with built-in fallback defaults when the file or keys are missing:
- `log_level` (string, e.g. `info`/`warn`/`error`)
- `window_width` (int)
- `window_height` (int)
- `window_show_ms` (int)
