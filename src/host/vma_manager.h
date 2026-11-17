/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef WINUX_VMA_MANAGER_H
#define WINUX_VMA_MANAGER_H
#include <stddef.h>
#include <stdint.h>
#include "memory_manager.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct winux_vma_manager winux_vma_manager;
typedef enum winux_vma_protection {
    WINUX_VMA_PROT_NONE = 0,
    WINUX_VMA_PROT_READ = 1u << 0,
    WINUX_VMA_PROT_WRITE = 1u << 1
} winux_vma_protection;
typedef enum winux_vma_kind {
    WINUX_VMA_ANONYMOUS_PRIVATE = 1
} winux_vma_kind;
typedef struct winux_vma_descriptor {
    size_t offset;
    size_t length;
    uint32_t protection;
    winux_vma_kind kind;
} winux_vma_descriptor;
/* Tracks mappings inside a reservation already owned by memory_manager. */
int winux_vma_manager_create(winux_memory_manager *memory, void *reservation_base,
    size_t reservation_size, winux_vma_manager **out_manager);
void winux_vma_manager_destroy(winux_vma_manager *manager);
/* First implementation supports anonymous/private mappings only. */
int winux_vma_map_anonymous_private(winux_vma_manager *manager,
    size_t offset, size_t length, uint32_t protection);
int winux_vma_protect(winux_vma_manager *manager,
    size_t offset, size_t length, uint32_t protection);
int winux_vma_unmap(winux_vma_manager *manager, size_t offset, size_t length);
/* Two-pass snapshot: insufficient capacity writes no partial output. */
int winux_vma_snapshot(winux_vma_manager *manager,
    winux_vma_descriptor *out_areas, size_t capacity, size_t *out_count);
#ifdef __cplusplus
}
#endif
#endif
