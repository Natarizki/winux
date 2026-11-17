# Kernel-hosting feasibility notes

**Status:** research note; no Linux kernel source has been imported into this repository.

## Why User-Mode Linux is a useful reference

The upstream Linux documentation describes User-Mode Linux (UML) as a Linux kernel built to run as a normal process on a Linux host. The host Linux kernel provides the process and system-call environment used by UML. The UML build is selected with ARCH=um.

The UML development documentation describes host mechanisms involved in its execution, including ptrace-based tracing, signals, epoll-based event handling, and host timers. These are not incidental utility calls: they are part of how UML controls transitions between its kernel and userspace execution and emulates machine facilities.

References:

- Linux kernel documentation, User Mode Linux HOWTO: https://www.kernel.org/doc/html/v5.7/virt/uml/user_mode_linux.html
- Linux kernel documentation, UML HowTo: https://docs.kernel.org/6.19/virt/uml/user_mode_linux_howto_v2.html
- Linux kernel paper, Design and implementation (describes UML's ptrace-based user/kernel separation): https://www.kernel.org/doc/ols/2001/uml.pdf

## Consequence for Winux

A prebuilt UML executable is not a native Windows executable, and a Linux-hosted UML binary cannot be assumed to run directly as a Win32 process. The Windows host does not provide Linux ptrace, Linux signal delivery, Linux fork/clone semantics, or the Linux host interfaces used by UML.

The current Winux Win32 backend implements only a small subset of generic host services:

- reserved/committed process virtual memory and protection changes;
- host threads and joins;
- events and mutexes;
- a monotonic millisecond clock.

These services are necessary building blocks, but they do not implement UML's execution-control model or a Linux kernel ABI.

## Main porting gaps to investigate

1. **Execution and syscall interception:** UML relies on host mechanisms such as ptrace to control execution. Windows debugging APIs are not a drop-in implementation of Linux ptrace semantics.
2. **Signals and exceptions:** synchronous faults, asynchronous notifications, signal masks, alternate signal stacks, and Linux signal delivery need an explicit Windows-hosted design.
3. **Process and address-space semantics:** fork/clone, copy-on-write, independent address spaces, and task lifecycle cannot be equated with simply starting a Win32 thread.
4. **Memory fault model:** VirtualAlloc and VirtualProtect provide page allocation and protection, but Winux still needs a defined way to turn Windows access violations into the kernel's expected fault and recovery paths.
5. **I/O and event model:** Linux interfaces such as epoll, file descriptors, and signal-driven I/O need a deliberate mapping to Windows handles, wait primitives, and possibly I/O completion facilities.
6. **Build and executable format:** UML's normal build expects a Linux host toolchain and produces a Linux-hosted executable. A native Windows deliverable needs a PE-compatible toolchain and a port of the host-side code.

## Candidate engineering direction

Keep the current code as the Win32 host backend and treat UML as a source-level architectural reference, not as a binary dependency.

Before importing kernel code:

- perform a source-level audit of the selected Linux kernel release's arch/um host dependencies;
- classify each dependency as directly mappable, mappable with semantic changes, or requiring a new design;
- decide whether the project will adapt UML's host-side code or maintain a dedicated Winux architecture/host port;
- record the exact upstream revision and preserve per-file license notices before copying any Linux source.

No Linux-kernel execution path exists yet. The current host API must not be mistaken for one.
