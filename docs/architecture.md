# Winux architecture (prototype stage)

## Goal

Winux investigates a native Windows process that can host an adapted Linux kernel execution environment. The design target excludes WSL, a conventional VM, and dual boot.

## Current implementation

```text
Windows NT
  └─ Winux.exe (native Windows process)
       ├─ C++ RuntimeController
       └─ C runtime
            └─ Win32 host primitives
                 └─ VirtualAlloc / VirtualFree / GetSystemInfo
```

The current implementation demonstrates only process lifecycle and host-memory ownership. It does not load Linux kernel code, implement Linux task semantics, or expose Linux system calls.

## Future kernel-hosting work

The kernel-hosting layer is a separate, substantial engineering effort. Before attempting to execute kernel code, the project needs explicit designs and tests for:

1. **Execution model:** mapping Linux task/thread concepts onto Windows scheduling primitives without pretending that ordinary Win32 threads are Linux tasks.
2. **Memory model:** a Linux virtual/physical memory abstraction backed by Windows process memory, with defined page, protection, and fault semantics.
3. **Kernel platform layer:** replacing or adapting assumptions about interrupts, timers, synchronization, CPU features, and low-level entry points.
4. **Device boundary:** a constrained interface for host-provided devices and I/O. No direct hardware access is implemented by this prototype.
5. **Syscall boundary:** a Linux userspace ABI only after a kernel execution backend exists.

These are design requirements, not completed features. A standard Linux kernel image cannot simply be linked into a Windows executable.

## Language boundaries

- **C:** low-level host boundary and runtime core.
- **C++17:** ownership, orchestration, and higher-level integration.
- **V:** not part of the bootstrap toolchain. It may be evaluated later for optional developer utilities, but it is not needed in the host/runtime core.

## Constraints

- Native Windows x64 is the first build target.
- The host process remains an ordinary Windows process.
- The prototype must report unsupported kernel features honestly rather than presenting placeholders as working Linux functionality.
