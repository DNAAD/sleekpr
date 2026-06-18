# sleekpr

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
