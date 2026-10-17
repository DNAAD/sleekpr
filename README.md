# sleekpr

当前发布版本专注于原生 Qt 标签打印，保留固定文本、绑定字段、二维码、矩形、数组网格、模板库、设备校准及本地 HTTP 接口。

复杂表格设计器暂缓开放，默认构建和正式绿色包不创建表格编辑面板，不启用画布合并、行带及分页编辑入口。历史模板中的表格数据继续保留，并通过既有 Qt 原生链路预览和打印，编辑其他标签元素不会删除这些数据。

表格编辑代码和回归测试暂存于原模块，开发构建可通过 `-DSLEEKPR_ENABLE_TABLE_DESIGNER=ON` 恢复；正式打包目标始终构建标签版本。`sleekpr_label_app_tests` 验证标签版本，`sleekpr_app_tests` 保留完整编辑器回归覆盖。

Native C++/Qt migration of the local ZYTXT print client.

## Current Scope

This repository starts as a structured native replacement for `dotnet-print`.
The first migration slice includes:

- Qt/CMake project layout.
- Core settings persistence compatible with the current JSON shape.
- Local CORS and Private Network Access policy logic.
- 80mm x 30mm label paper and print-unit conversion.
- Deterministic label render planning and millimeter-based drawing commands.
- A native Qt application shell with a local HTTP service on `127.0.0.1:37122`.

## Build

Install Qt 6.5+ and CMake 3.24+, then run:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:\Qt\6.5.0\msvc2019_64"
cmake --build build
ctest --test-dir build --output-on-failure
```

Adjust `CMAKE_PREFIX_PATH` to your local Qt installation.

## Architecture

- `include/sleekpr/core` and `src/core`: testable domain logic.
- `include/sleekpr/http` and `src/http`: local HTTP compatibility layer.
- `src/app`: Qt application startup and tray lifecycle.
- `tests`: Qt Test based regression coverage.

See `AGENTS.md` for mandatory engineering constraints.
