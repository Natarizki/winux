/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Pure policy layer for observed host memory faults.
 */
#include "fault_dispatcher.h"

#include <stdint.h>

static int winux_region_end(
    const winux_memory_region_descriptor *region,
    uintptr_t *out_end
)
{
    if (region == NULL || out_end == NULL || region->size == 0u ||
        (region->flags & ~((uint32_t)WINUX_MEMORY_REGION_VALID_FLAGS)) != 0u ||
        region->size > (size_t)(UINTPTR_MAX - region->base)) {
        return 0;
    }

    *out_end = region->base + (uintptr_t)region->size;
    return 1;
}

static int winux_regions_valid(
    const winux_memory_region_descriptor *regions,
    size_t count
)
{
    size_t i;
    size_t j;
    uintptr_t i_end;
    uintptr_t j_end;

    if (count != 0u && regions == NULL) {
        return 0;
    }

    for (i = 0u; i < count; ++i) {
        if (!winux_region_end(&regions[i], &i_end)) {
            return 0;
        }
        for (j = i + 1u; j < count; ++j) {
            if (!winux_region_end(&regions[j], &j_end)) {
                return 0;
            }
            if (regions[i].base < j_end && regions[j].base < i_end) {
                return 0;
            }
        }
    }

    return 1;
}

int winux_fault_dispatcher_classify(
    const winux_host_fault_record *fault,
    const winux_memory_region_descriptor *regions,
    size_t region_count,
    winux_fault_decision *out_decision
)
{
    size_t i;
    uintptr_t end;
    const winux_memory_region_descriptor *match = NULL;

    if (fault == NULL || out_decision == NULL ||
        !winux_regions_valid(regions, region_count)) {
        return 0;
    }

    out_decision->disposition = WINUX_FAULT_DISPOSITION_INVALID;
    out_decision->region_index = SIZE_MAX;
    out_decision->fault_address = fault->fault_address;
    out_decision->access = fault->access;

    if (fault->kind != WINUX_HOST_FAULT_ACCESS_VIOLATION &&
        fault->kind != WINUX_HOST_FAULT_GUARD_PAGE) {
        out_decision->disposition = WINUX_FAULT_DISPOSITION_UNSUPPORTED;
        return 1;
    }

    for (i = 0u; i < region_count; ++i) {
        (void)winux_region_end(&regions[i], &end);
        if (fault->fault_address >= regions[i].base &&
            fault->fault_address < end) {
            match = &regions[i];
            out_decision->region_index = i;
            break;
        }
    }

    if (match == NULL || (match->flags & WINUX_MEMORY_REGION_COMMITTED) == 0u) {
        out_decision->disposition = WINUX_FAULT_DISPOSITION_UNMAPPED;
        return 1;
    }

    if (fault->kind == WINUX_HOST_FAULT_GUARD_PAGE ||
        (match->flags & WINUX_MEMORY_REGION_GUARD) != 0u) {
        out_decision->disposition = WINUX_FAULT_DISPOSITION_GUARD_PAGE;
        return 1;
    }

    if (fault->access == WINUX_HOST_FAULT_ACCESS_UNKNOWN) {
        out_decision->disposition = WINUX_FAULT_DISPOSITION_UNKNOWN_ACCESS;
        return 1;
    }

    if ((fault->access == WINUX_HOST_FAULT_ACCESS_READ &&
         (match->flags & WINUX_MEMORY_REGION_READ) == 0u) ||
        (fault->access == WINUX_HOST_FAULT_ACCESS_WRITE &&
         (match->flags & WINUX_MEMORY_REGION_WRITE) == 0u) ||
        (fault->access == WINUX_HOST_FAULT_ACCESS_EXECUTE &&
         (match->flags & WINUX_MEMORY_REGION_EXECUTE) == 0u)) {
        out_decision->disposition = WINUX_FAULT_DISPOSITION_PROTECTION_VIOLATION;
        return 1;
    }

    out_decision->disposition = WINUX_FAULT_DISPOSITION_UNEXPECTED_HOST_FAULT;
    return 1;
}
