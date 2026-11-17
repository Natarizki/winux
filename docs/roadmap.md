# Roadmap

## Stage 0 — Repository and host-runtime scaffold
- [x] CMake project with C and C++ targets
- [x] Native Win32 memory allocation/release wrapper
- [x] C runtime lifecycle API
- [x] C++ RAII controller
- [x] Windows CI build and lifecycle tests

## Stage 1 — Host platform research
- [ ] Document supported Windows versions and CPU assumptions
- [ ] Specify host thread, timer, synchronization, and memory abstractions
- [ ] Evaluate user-mode kernel execution approaches and their host dependencies
- [ ] Define a minimal, testable kernel-host interface

## Stage 2 — Kernel-hosting proof of concept
- [ ] Establish a legally and technically suitable Linux kernel source baseline
- [ ] Identify architecture-specific code that cannot run in a normal Win32 process
- [ ] Implement one narrowly scoped host primitive at a time
- [ ] Demonstrate a controlled kernel execution path before claiming Linux compatibility

## Stage 3 — Linux userspace compatibility
- [ ] Define syscall and signal boundaries
- [ ] Add a minimal userspace test suite
- [ ] Add filesystem and process integration only after the kernel backend is functional

No roadmap item beyond Stage 0 is claimed as implemented.
