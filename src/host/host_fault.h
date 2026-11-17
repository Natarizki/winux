/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Host exception/fault observation contract.
 */
#ifndef WINUX_HOST_FAULT_H
#define WINUX_HOST_FAULT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum winux_host_fault_kind {
    WINUX_HOST_FAULT_NONE = 0,
    WINUX_HOST_FAULT_ACCESS_VIOLATION,
    WINUX_HOST_FAULT_GUARD_PAGE,
    WINUX_HOST_FAULT_OTHER
} winux_host_fault_kind;

typedef enum winux_host_fault_access {
    WINUX_HOST_FAULT_ACCESS_UNKNOWN = 0,
    WINUX_HOST_FAULT_ACCESS_READ,
    WINUX_HOST_FAULT_ACCESS_WRITE,
    WINUX_HOST_FAULT_ACCESS_EXECUTE
} winux_host_fault_access;

typedef struct winux_host_fault_record {
    winux_host_fault_kind kind;
    winux_host_fault_access access;
    uintptr_t fault_address;
    uint32_t platform_exception_code;
} winux_host_fault_record;

/*
 * Reads one byte under the Windows host exception boundary.
 * Returns 1 on success, 0 when a supported memory fault was observed,
 * and -1 for invalid arguments. This is diagnostic host probing only;
 * it is not a Linux page-fault handler.
 */
int winux_host_memory_probe_read(
    const volatile void *address,
    uint8_t *out_value,
    winux_host_fault_record *out_fault
);

#ifdef __cplusplus
}
#endif

#endif /* WINUX_HOST_FAULT_H */
