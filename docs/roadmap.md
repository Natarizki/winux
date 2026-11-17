# Roadmap

## Stage 0 — Repository and runtime scaffold
- [x] CMake project with C and C++ targets
- [x] Native Win32 runtime lifecycle
- [x] C++ RAII controller and CLI
- [x] Windows CI build, tests, and executable artifact

## Stage 1 — Win32 host primitives
- [x] Reserved virtual-memory regions with commit/decommit
- [x] Memory protection changes for no-access, read-only, and read-write pages
- [x] Host thread create/join lifecycle
- [x] Event and mutex synchronization wrappers
- [x] Monotonic millisecond clock
- [x] Windows CI coverage for host primitives
- [x] Define a platform-neutral C host API with opaque handles and no Win32 types
- [x] Specify cooperative thread stop and join semantics (host primitive; no forced termination)
- [x] Research exception, fault, and signal translation constraints
- [x] Add a narrow MSVC SEH memory-probe boundary with fault metadata and tests (host diagnostic only; not Linux page-fault handling)
- [x] Add a pure memory-fault classifier for mapped state, guard pages, access permissions, and unknown regions
- [x] Add an owned host virtual-memory registry with per-page metadata and coalesced snapshots

## Stage 2 — Kernel-hosting feasibility prototype
- [ ] Select and document a suitable Linux kernel source baseline and architecture
- [ ] Map the required kernel host dependencies and quantify the porting boundary
- [ ] Design an isolated kernel execution context and memory model
- [ ] Implement one narrowly scoped kernel-facing primitive at a time
- [ ] Demonstrate an actual controlled kernel execution path before claiming Linux compatibility

## Stage 3 — Linux userspace compatibility
- [ ] Define syscall, signal, process, and filesystem boundaries
- [ ] Add a minimal Linux userspace ABI test suite
- [ ] Add process and I/O integration only after the kernel backend is functional

Only completed and CI-tested items are checked. Stages 2 and 3 are not implemented.


The fault classifier consumes a caller-owned region snapshot. It is policy
classification only; it does not repair faults or implement Linux MM behavior.
