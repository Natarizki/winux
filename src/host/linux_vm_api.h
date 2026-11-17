/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef WINUX_LINUX_VM_API_H
#define WINUX_LINUX_VM_API_H

#include <stddef.h>
#include <stdint.h>
#include "memory_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct winux_linux_vm_api winux_linux_vm_api;

/* Linux-compatible flag values for the deliberately small supported subset. */
#define WINUX_LINUX_PROT_NONE  0x0
#define WINUX_LINUX_PROT_READ  0x1
#define WINUX_LINUX_PROT_WRITE 0x2
#define WINUX_LINUX_PROT_EXEC  0x4

#define WINUX_LINUX_MAP_SHARED    0x01
#define WINUX_LINUX_MAP_PRIVATE   0x02
#define WINUX_LINUX_MAP_FIXED     0x10
#define WINUX_LINUX_MAP_ANONYMOUS 0x20

#define WINUX_LINUX_EACCES 13
#define WINUX_LINUX_EFAULT 14
#define WINUX_LINUX_EINVAL 22
#define WINUX_LINUX_ENOMEM 12
#define WINUX_LINUX_ENOTSUP 95

#define WINUX_LINUX_MAP_FAILED ((void *)(intptr_t)-1)

/*
 * A small Linux-shaped VM API facade over a caller-owned Win32 reservation.
 * It is not a syscall dispatcher and does not expose a Linux process ABI.
 */
int winux_linux_vm_api_create(
    winux_memory_manager *memory,
    void *reservation_base,
    size_t reservation_size,
    winux_linux_vm_api **out_api);
void winux_linux_vm_api_destroy(winux_linux_vm_api *api);

/*
 * Supports only MAP_PRIVATE | MAP_ANONYMOUS, fd == -1, offset == 0.
 * Address hints are advisory and ignored; the first suitable gap is chosen.
 * On failure returns WINUX_LINUX_MAP_FAILED and writes a positive errno value.
 */
void *winux_linux_mmap(
    winux_linux_vm_api *api,
    void *address_hint,
    size_t length,
    int protection,
    int flags,
    int fd,
    uint64_t offset,
    int *out_error);

/* Address must be page-aligned. Length is rounded up to a page boundary. */
int winux_linux_munmap(
    winux_linux_vm_api *api,
    void *address,
    size_t length,
    int *out_error);
int winux_linux_mprotect(
    winux_linux_vm_api *api,
    void *address,
    size_t length,
    int protection,
    int *out_error);

#ifdef __cplusplus
}
#endif
#endif
