/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Platform-neutral memory fault classification policy.
 */
#ifndef WINUX_FAULT_DISPATCHER_H
#define WINUX_FAULT_DISPATCHER_H

#include <stddef.h>
#include <stdint.h>
#include "host_fault.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum winux_memory_region_flags {
    WINUX_MEMORY_REGION_COMMITTED = 1u << 0,
    WINUX_MEMORY_REGION_READ      = 1u << 1,
    WINUX_MEMORY_REGION_WRITE     = 1u << 2,
    WINUX_MEMORY_REGION_EXECUTE   = 1u << 3,
    WINUX_MEMORY_REGION_GUARD     = 1u << 4
} winux_memory_region_flags;

#define WINUX_MEMORY_REGION_VALID_FLAGS \
    (WINUX_MEMORY_REGION_COMMITTED | WINUX_MEMORY_REGION_READ | \
     WINUX_MEMORY_REGION_WRITE | WINUX_MEMORY_REGION_EXECUTE | \
     WINUX_MEMORY_REGION_GUARD)

typedef struct winux_memory_region_descriptor {
    uintptr_t base;
    size_t size;
    uint32_t flags;
} winux_memory_region_descriptor;

typedef enum winux_fault_disposition {
    WINUX_FAULT_DISPOSITION_INVALID = 0,
    WINUX_FAULT_DISPOSITION_UNSUPPORTED,
    WINUX_FAULT_DISPOSITION_UNMAPPED,
    WINUX_FAULT_DISPOSITION_GUARD_PAGE,
    WINUX_FAULT_DISPOSITION_PROTECTION_VIOLATION,
    WINUX_FAULT_DISPOSITION_UNKNOWN_ACCESS,
    WINUX_FAULT_DISPOSITION_UNEXPECTED_HOST_FAULT
} winux_fault_disposition;

typedef struct winux_fault_decision {
    winux_fault_disposition disposition;
    size_t region_index;
    uintptr_t fault_address;
    winux_host_fault_access access;
} winux_fault_decision;

/*
 * Classifies an observed host fault against a caller-owned address-space
 * snapshot. Regions must be non-empty, non-overlapping, and non-overflowing.
 * Returns 1 when a decision is produced, or 0 for invalid input/metadata.
 * This function never changes memory protections or resumes execution.
 */
int winux_fault_dispatcher_classify(
    const winux_host_fault_record *fault,
    const winux_memory_region_descriptor *regions,
    size_t region_count,
    winux_fault_decision *out_decision
);

#ifdef __cplusplus
}
#endif

#endif /* WINUX_FAULT_DISPATCHER_H */
