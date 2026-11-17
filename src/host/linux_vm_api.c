/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "linux_vm_api.h"
#include "vma_manager.h"
#include "host_api.h"
#include <stdint.h>
#include <stdlib.h>

struct winux_linux_vm_api {
    winux_vma_manager *vma;
    uintptr_t base;
    size_t size;
    size_t page_size;
};

static int round_length(size_t length, size_t page, size_t *out) {
    size_t rem;
    if (!length || !page || !out) return 0;
    rem = length % page;
    if (!rem) { *out = length; return 1; }
    if (length > SIZE_MAX - (page - rem)) return 0;
    *out = length + (page - rem);
    return 1;
}

static int convert_protection(int protection, uint32_t *out) {
    if (!out || (protection & ~(WINUX_LINUX_PROT_READ |
                                WINUX_LINUX_PROT_WRITE |
                                WINUX_LINUX_PROT_EXEC)) != 0)
        return 0;
    /* The current host backend intentionally has no executable-page support. */
    if ((protection & WINUX_LINUX_PROT_EXEC) != 0) return -1;
    if ((protection & WINUX_LINUX_PROT_WRITE) &&
        !(protection & WINUX_LINUX_PROT_READ)) return 0;
    *out = (protection & WINUX_LINUX_PROT_WRITE)
        ? (WINUX_VMA_PROT_READ | WINUX_VMA_PROT_WRITE)
        : ((protection & WINUX_LINUX_PROT_READ)
            ? WINUX_VMA_PROT_READ : WINUX_VMA_PROT_NONE);
    return 1;
}

static int address_to_offset(
    winux_linux_vm_api *api, void *address, size_t length,
    size_t *out_offset, size_t *out_length) {
    uintptr_t value, end;
    size_t rounded;
    if (!api || !address || !out_offset || !out_length ||
        ((uintptr_t)address % api->page_size) != 0 ||
        !round_length(length, api->page_size, &rounded))
        return 0;
    value = (uintptr_t)address;
    if (value < api->base) return 0;
    if (value > UINTPTR_MAX - rounded) return 0;
    end = value + rounded;
    if (end > api->base + api->size) return 0;
    *out_offset = (size_t)(value - api->base);
    *out_length = rounded;
    return 1;
}

int winux_linux_vm_api_create(
    winux_memory_manager *memory, void *base, size_t size,
    winux_linux_vm_api **out_api) {
    winux_linux_vm_api *api;
    if (!out_api) return 0;
    *out_api = NULL;
    if (!memory || !base || !size || !winux_host_page_size()) return 0;
    api = (winux_linux_vm_api *)calloc(1u, sizeof(*api));
    if (!api) return 0;
    api->base = (uintptr_t)base;
    api->size = size;
    api->page_size = (size_t)winux_host_page_size();
    if (api->base > UINTPTR_MAX - size ||
        size % api->page_size != 0 ||
        !winux_vma_manager_create(memory, base, size, &api->vma)) {
        free(api);
        return 0;
    }
    *out_api = api;
    return 1;
}

void winux_linux_vm_api_destroy(winux_linux_vm_api *api) {
    if (!api) return;
    winux_vma_manager_destroy(api->vma);
    free(api);
}

void *winux_linux_mmap(
    winux_linux_vm_api *api, void *hint, size_t length,
    int protection, int flags, int fd, uint64_t offset, int *out_error) {
    size_t rounded, count = 0, i, cursor = 0;
    winux_vma_descriptor *areas = NULL;
    uint32_t vma_protection;
    int converted;
    (void)hint;
    if (out_error) *out_error = 0;
    if (!api || !round_length(length, api ? api->page_size : 0, &rounded) ||
        (flags != (WINUX_LINUX_MAP_PRIVATE | WINUX_LINUX_MAP_ANONYMOUS)) ||
        fd != -1 || offset != 0) {
        if (out_error) *out_error = WINUX_LINUX_EINVAL;
        return WINUX_LINUX_MAP_FAILED;
    }
    converted = convert_protection(protection, &vma_protection);
    if (converted <= 0) {
        if (out_error) *out_error = converted < 0
            ? WINUX_LINUX_ENOTSUP : WINUX_LINUX_EINVAL;
        return WINUX_LINUX_MAP_FAILED;
    }
    if (rounded > api->size) {
        if (out_error) *out_error = WINUX_LINUX_ENOMEM;
        return WINUX_LINUX_MAP_FAILED;
    }
    if (!winux_vma_snapshot(api->vma, NULL, 0, &count) && count != 0) {
        areas = (winux_vma_descriptor *)malloc(count * sizeof(*areas));
        if (!areas) {
            if (out_error) *out_error = WINUX_LINUX_ENOMEM;
            return WINUX_LINUX_MAP_FAILED;
        }
        if (!winux_vma_snapshot(api->vma, areas, count, &count)) {
            free(areas);
            if (out_error) *out_error = WINUX_LINUX_ENOMEM;
            return WINUX_LINUX_MAP_FAILED;
        }
    }
    for (i = 0; i < count; ++i) {
        if (cursor <= areas[i].offset &&
            rounded <= areas[i].offset - cursor) break;
        if (areas[i].offset <= cursor &&
            areas[i].length <= api->size - cursor)
            cursor = areas[i].offset + areas[i].length;
    }
    free(areas);
    if (cursor > api->size || rounded > api->size - cursor ||
        !winux_vma_map_anonymous_private(
            api->vma, cursor, rounded, vma_protection)) {
        if (out_error) *out_error = WINUX_LINUX_ENOMEM;
        return WINUX_LINUX_MAP_FAILED;
    }
    return (void *)(api->base + cursor);
}

int winux_linux_munmap(
    winux_linux_vm_api *api, void *address, size_t length, int *out_error) {
    size_t offset, rounded;
    if (out_error) *out_error = 0;
    if (!address_to_offset(api, address, length, &offset, &rounded)) {
        if (out_error) *out_error = WINUX_LINUX_EINVAL;
        return -1;
    }
    if (!winux_vma_unmap(api->vma, offset, rounded)) {
        if (out_error) *out_error = WINUX_LINUX_EINVAL;
        return -1;
    }
    return 0;
}

int winux_linux_mprotect(
    winux_linux_vm_api *api, void *address, size_t length,
    int protection, int *out_error) {
    size_t offset, rounded;
    uint32_t vma_protection;
    int converted;
    if (out_error) *out_error = 0;
    if (!address_to_offset(api, address, length, &offset, &rounded)) {
        if (out_error) *out_error = WINUX_LINUX_EINVAL;
        return -1;
    }
    converted = convert_protection(protection, &vma_protection);
    if (converted <= 0) {
        if (out_error) *out_error = converted < 0
            ? WINUX_LINUX_ENOTSUP : WINUX_LINUX_EINVAL;
        return -1;
    }
    if (!winux_vma_protect(api->vma, offset, rounded, vma_protection)) {
        if (out_error) *out_error = WINUX_LINUX_EINVAL;
        return -1;
    }
    return 0;
}
