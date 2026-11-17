/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Owned host virtual-memory region registry.
 */
#ifndef WINUX_MEMORY_MANAGER_H
#define WINUX_MEMORY_MANAGER_H
#include <stddef.h>
#include <stdint.h>
#include "host_api.h"
#include "fault_dispatcher.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct winux_memory_manager winux_memory_manager;
int winux_memory_manager_create(winux_memory_manager **out_manager);
void winux_memory_manager_destroy(winux_memory_manager *manager);
int winux_memory_manager_reserve(winux_memory_manager *manager, size_t bytes, winux_host_memory_region *out_region);
int winux_memory_manager_commit(winux_memory_manager *manager, void *region_base, size_t offset, size_t bytes);
int winux_memory_manager_decommit(winux_memory_manager *manager, void *region_base, size_t offset, size_t bytes);
int winux_memory_manager_protect(winux_memory_manager *manager, void *region_base, size_t offset, size_t bytes, winux_host_memory_protection protection);
int winux_memory_manager_release(winux_memory_manager *manager, void *region_base);
/* out_count receives required count; insufficient capacity writes no partial snapshot. */
int winux_memory_manager_snapshot(winux_memory_manager *manager, winux_memory_region_descriptor *out_regions, size_t capacity, size_t *out_count);
#ifdef __cplusplus
}
#endif
#endif
