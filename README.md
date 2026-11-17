# Winux

**Native Linux Kernel Runtime for Windows** — an experimental project exploring how Linux kernel code could be hosted by a native Windows process, without WSL, a traditional virtual machine, or dual boot.

> **Status: early prototype.** The current code is a Windows host-runtime skeleton. It does **not** contain, boot, or execute the Linux kernel yet. It does not implement Linux system calls, a Linux ABI, device drivers, or hardware virtualization.

## Current prototype

- Native Win32 process (C backend).
- C API for runtime lifecycle and host-memory management.
- C++17 RAII controller and command-line executable.
- CMake build and Windows x64 CI.
- Lifecycle tests run by GitHub Actions.

The memory arena allocated by this prototype is only host-managed memory. It is **not** a Linux guest-physical-memory implementation and is not used to execute kernel code.

## Build

Requirements: Windows 10/11 x64, Visual Studio 2022 C++ build tools, and CMake 3.21 or newer.

```powershell
cmake -S . -B build -A x64 -DWINUX_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
.\build\Release\winux.exe
```

## Project layout

- `include/winux/` — public C API.
- `src/host/` — Windows host primitives.
- `src/runtime/` — runtime lifecycle.
- `src/integration/` — C++ ownership/controller layer.
- `src/cli/` — command-line entry point.
- `tests/` — runtime lifecycle tests.
- `docs/` — architecture and implementation roadmap.

## Direction

The intended research direction is a user-mode Linux-kernel hosting backend. That requires a substantial port/adaptation of kernel-facing facilities (memory management, task scheduling, interrupts, timers, synchronization, and device interfaces) to a Windows host environment. A normal Linux kernel cannot simply be linked into a Windows process. The prototype keeps that work separate from the small, testable Win32 host layer.

## License

The repository includes the GNU General Public License, version 2 or (at your option) any later version. See [LICENSE](LICENSE). Individual imported components may carry additional license and notice requirements.
