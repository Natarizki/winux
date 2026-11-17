# Winux architecture (prototype stage)

## Goal

Winux investigates whether an adapted Linux kernel execution environment can be hosted by a native Windows process. The design target excludes WSL, a conventional virtual machine, and dual boot.

## Current implementation

    Windows NT
      └─ Winux.exe (native Windows process)
           ├─ C++ RuntimeController
           └─ C runtime
                └─ platform-neutral host_api.h contract
                     └─ Win32 host backend
                     ├─ VirtualAlloc / VirtualProtect / VirtualFree
                     ├─ CreateThread / WaitForSingleObject
                     ├─ Events and mutexes
                     └─ GetTickCount64

Implemented host primitives:

- **Virtual memory regions:** reserve, commit, decommit, protect (no-access, read-only, read-write), and release. Ranges must be page-aligned and remain inside the reserved region. Executable page permissions are intentionally not exposed.
- **Threads:** create, request cooperative stop, poll a read-only stop token, join, obtain exit code, and destroy a host thread handle. Stop requests do not forcibly terminate a worker; callbacks must poll the token and return. Destroy waits for a worker that has not exited.
- **Synchronization:** manual/auto-reset events and mutexes.
- **Monotonic clock:** millisecond counter from the Windows host.
- **Fault observation:** an MSVC SEH boundary can probe a byte and capture access-violation metadata (fault address and read/write/execute classification). This is a narrow diagnostic boundary, not a general exception dispatcher or Linux page-fault implementation.

The fault probe catches only access violations and guard-page exceptions; unrelated structured exceptions continue searching rather than being swallowed. The current boundary is MSVC-specific and deliberately does not expose executable-page permissions. It does not resume or repair a faulting instruction.

The stop token is backed by an interlocked host flag; it is a lifecycle primitive, not Linux task cancellation or signal delivery. These are wrappers around Win32 process resources. They are not Linux mm, Linux tasks, Linux wait queues, Linux futexes, or a kernel scheduler. In particular, a Win32 thread is only a possible host execution resource; it is not automatically equivalent to a Linux task.

## What is not implemented

The current code does not load or execute Linux kernel code, implement Linux task semantics, deliver Linux signals, handle Linux page faults, expose Linux system calls, or provide Linux device drivers. The memory regions are ordinary process virtual memory, not a Linux guest-physical-memory system.

The host contract is defined in src/host/host_api.h and uses opaque handles rather than exposing Win32 types. The current implementation is in src/host/win32_platform.c. This separation permits a future backend without changing the runtime-facing API.

## Future kernel-hosting work

A kernel-hosting layer remains a substantial port/adaptation effort. Before executing kernel code, the project needs explicit designs and tests for:

1. **Execution model:** map Linux task/thread concepts onto Windows scheduling primitives, including task state, context switching constraints, and host-thread lifetime.
2. **Memory model:** define Linux virtual/physical memory semantics, page faults, mapping, protection, and allocation over Windows process memory.
3. **Exception and signal model:** define how synchronous CPU exceptions and Linux signal semantics can be represented without unsafe assumptions about Windows exception handling.
4. **Kernel platform layer:** adapt timers, synchronization, CPU features, low-level entry points, and assumptions about interrupts.
5. **Device and I/O boundary:** define constrained host-provided devices and file/network I/O interfaces.
6. **Syscall boundary:** add a Linux userspace ABI only after a kernel execution backend exists.

These are research and implementation requirements, not completed features. A standard Linux kernel image cannot simply be linked into a Windows executable.

## Language boundaries

- **C11:** low-level host boundary and runtime core.
- **C++17:** ownership, orchestration, and higher-level integration.
- **V:** not part of the bootstrap toolchain. It may be evaluated for optional developer tools later; it is not needed in the host/runtime core.

## Constraints

- Native Windows x64 is the first target.
- The host process remains an ordinary Windows process.
- No WSL or conventional virtual machine is part of the target architecture.
- Unsupported kernel features must be reported honestly, not represented by placeholders.


## Fault classification policy

The host SEH probe feeds a separate, platform-neutral dispatcher in
`src/host/fault_dispatcher.c`. The dispatcher accepts a caller-owned snapshot
of non-overlapping address regions and classifies an observed fault as
unmapped, guard-page, protection violation, unknown access, unsupported, or
unexpected within an allowed region. Region metadata describes policy; it is
not yet synchronized with the Win32 virtual-memory manager.

The dispatcher is deliberately pure: it does not allocate memory, change page
protections, retry an access, or resume a faulting instruction. A future memory
manager must own and validate the region snapshot before using this policy.


## Owned memory-region manager

The memory manager owns a registry of reserved host virtual-memory regions.
Each reservation has per-page state; snapshots coalesce adjacent pages with
identical flags and are sorted by address for the fault classifier. Operations
are serialized by a host mutex. Metadata changes occur only after the
corresponding host operation succeeds. Callers must not mutate registered
regions through the lower-level host memory API.

This remains process virtual memory. It does not implement Linux VMAs,
copy-on-write, page tables, physical frames, executable mappings, demand
paging, or guest memory.
