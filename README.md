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

### Config
Configuration is loaded from `configs/default.json` with built-in fallback defaults when the file or keys are missing:
- `log_level` (string, e.g. `info`/`warn`/`error`)
- `window_width` (int)
- `window_height` (int)
- `window_show_ms` (int)
