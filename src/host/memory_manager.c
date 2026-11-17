/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Owns and tracks process-local host virtual-memory reservations.
 */
#include "memory_manager.h"
#include "host_api.h"
#include <stdlib.h>

struct winux_managed_region {
    winux_host_memory_region host_region;
    uint32_t *page_flags;
    size_t page_count;
    struct winux_managed_region *next;
};
struct winux_memory_manager {
    winux_host_mutex *mutex;
    size_t page_size;
    struct winux_managed_region *regions;
};
static int manager_lock(winux_memory_manager *m) {
    uint32_t error = 0u;
    return winux_host_mutex_lock(m->mutex, &error);
}
static void manager_unlock(winux_memory_manager *m) {
    uint32_t error = 0u;
    (void)winux_host_mutex_unlock(m->mutex, &error);
}
static struct winux_managed_region *find_region(winux_memory_manager *m, void *base) {
    struct winux_managed_region *p;
    for (p = m->regions; p != NULL; p = p->next)
        if (p->host_region.base == base) return p;
    return NULL;
}
static int range_valid(const winux_memory_manager *m,
    const struct winux_managed_region *r, size_t offset, size_t bytes) {
    return r != NULL && bytes != 0u &&
        offset % m->page_size == 0u && bytes % m->page_size == 0u &&
        offset <= r->host_region.size && bytes <= r->host_region.size - offset;
}
int winux_memory_manager_create(winux_memory_manager **out_manager) {
    winux_memory_manager *m;
    uint32_t error = 0u;
    if (out_manager == NULL) return 0;
    *out_manager = NULL;
    m = (winux_memory_manager *)calloc(1u, sizeof(*m));
    if (m == NULL) return 0;
    m->page_size = (size_t)winux_host_page_size();
    if (m->page_size == 0u || !winux_host_mutex_create(&m->mutex, &error)) {
        free(m); return 0;
    }
    *out_manager = m;
    return 1;
}
void winux_memory_manager_destroy(winux_memory_manager *m) {
    struct winux_managed_region *p, *next;
    if (m == NULL) return;
    if (manager_lock(m)) {
        p = m->regions; m->regions = NULL;
        while (p != NULL) {
            next = p->next;
            (void)winux_host_memory_release(&p->host_region, NULL);
            free(p->page_flags); free(p); p = next;
        }
        manager_unlock(m);
    }
    winux_host_mutex_destroy(m->mutex);
    free(m);
}
int winux_memory_manager_reserve(winux_memory_manager *m, size_t bytes,
    winux_host_memory_region *out_region) {
    struct winux_managed_region *p;
    uint32_t error = 0u;
    if (m == NULL || out_region == NULL || bytes == 0u || bytes % m->page_size != 0u) return 0;
    out_region->base = NULL; out_region->size = 0u;
    p = (struct winux_managed_region *)calloc(1u, sizeof(*p));
    if (p == NULL) return 0;
    p->page_count = bytes / m->page_size;
    if (p->page_count > SIZE_MAX / sizeof(*p->page_flags)) { free(p); return 0; }
    p->page_flags = (uint32_t *)calloc(p->page_count, sizeof(*p->page_flags));
    if (p->page_flags == NULL) { free(p); return 0; }
    if (!manager_lock(m)) { free(p->page_flags); free(p); return 0; }
    if (!winux_host_memory_reserve(bytes, &p->host_region, &error)) {
        manager_unlock(m); free(p->page_flags); free(p); return 0;
    }
    p->next = m->regions; m->regions = p;
    *out_region = p->host_region;
    manager_unlock(m);
    return 1;
}
static int change_range(winux_memory_manager *m, void *base, size_t offset,
    size_t bytes, int operation, winux_host_memory_protection protection) {
    struct winux_managed_region *p;
    uint32_t error = 0u;
    size_t first, count, i;
    int ok;
    if (m == NULL || base == NULL || !manager_lock(m)) return 0;
    p = find_region(m, base);
    if (!range_valid(m, p, offset, bytes)) { manager_unlock(m); return 0; }
    if (operation == 0) ok = winux_host_memory_commit(&p->host_region, offset, bytes, &error);
    else if (operation == 1) ok = winux_host_memory_decommit(&p->host_region, offset, bytes, &error);
    else ok = winux_host_memory_protect(&p->host_region, offset, bytes, protection, &error);
    if (ok) {
        first = offset / m->page_size; count = bytes / m->page_size;
        for (i = first; i < first + count; ++i) {
            if (operation == 1) p->page_flags[i] = 0u;
            else if (operation == 0) p->page_flags[i] = WINUX_MEMORY_REGION_COMMITTED |
                WINUX_MEMORY_REGION_READ | WINUX_MEMORY_REGION_WRITE;
            else if (protection == WINUX_HOST_MEMORY_NO_ACCESS)
                p->page_flags[i] = WINUX_MEMORY_REGION_COMMITTED;
            else if (protection == WINUX_HOST_MEMORY_READ_ONLY)
                p->page_flags[i] = WINUX_MEMORY_REGION_COMMITTED | WINUX_MEMORY_REGION_READ;
            else p->page_flags[i] = WINUX_MEMORY_REGION_COMMITTED |
                WINUX_MEMORY_REGION_READ | WINUX_MEMORY_REGION_WRITE;
        }
    }
    manager_unlock(m);
    return ok;
}
int winux_memory_manager_commit(winux_memory_manager *m, void *b, size_t o, size_t n) {
    return change_range(m, b, o, n, 0, WINUX_HOST_MEMORY_NO_ACCESS);
}
int winux_memory_manager_decommit(winux_memory_manager *m, void *b, size_t o, size_t n) {
    return change_range(m, b, o, n, 1, WINUX_HOST_MEMORY_NO_ACCESS);
}
int winux_memory_manager_protect(winux_memory_manager *m, void *b, size_t o,
    size_t n, winux_host_memory_protection protection) {
    if (protection != WINUX_HOST_MEMORY_NO_ACCESS &&
        protection != WINUX_HOST_MEMORY_READ_ONLY &&
        protection != WINUX_HOST_MEMORY_READ_WRITE) return 0;
    return change_range(m, b, o, n, 2, protection);
}
int winux_memory_manager_release(winux_memory_manager *m, void *base) {
    struct winux_managed_region **link, *p;
    uint32_t error = 0u;
    if (m == NULL || base == NULL || !manager_lock(m)) return 0;
    for (link = &m->regions; *link != NULL; link = &(*link)->next)
        if ((*link)->host_region.base == base) break;
    p = *link;
    if (p == NULL || !winux_host_memory_release(&p->host_region, &error)) {
        manager_unlock(m); return 0;
    }
    *link = p->next;
    free(p->page_flags); free(p);
    manager_unlock(m);
    return 1;
}
static int descriptor_compare(const void *left, const void *right) {
    const winux_memory_region_descriptor *a = (const winux_memory_region_descriptor *)left;
    const winux_memory_region_descriptor *b = (const winux_memory_region_descriptor *)right;
    return a->base < b->base ? -1 : (a->base > b->base ? 1 : 0);
}
int winux_memory_manager_snapshot(winux_memory_manager *m,
    winux_memory_region_descriptor *out, size_t capacity, size_t *out_count) {
    struct winux_managed_region *p;
    size_t required = 0u, i, start, used = 0u;
    uint32_t flags;
    if (out_count == NULL || m == NULL || (capacity != 0u && out == NULL) || !manager_lock(m)) return 0;
    for (p = m->regions; p != NULL; p = p->next) {
        if (p->page_count == 0u) continue;
        ++required;
        for (i = 1u; i < p->page_count; ++i)
            if (p->page_flags[i] != p->page_flags[i - 1u]) ++required;
    }
    *out_count = required;
    if (capacity < required) { manager_unlock(m); return 0; }
    for (p = m->regions; p != NULL; p = p->next) {
        if (p->page_count == 0u) continue;
        start = 0u; flags = p->page_flags[0];
        for (i = 1u; i <= p->page_count; ++i) {
            if (i == p->page_count || p->page_flags[i] != flags) {
                out[used].base = (uintptr_t)p->host_region.base + start * m->page_size;
                out[used].size = (i - start) * m->page_size;
                out[used].flags = flags; ++used;
                if (i < p->page_count) { start = i; flags = p->page_flags[i]; }
            }
        }
    }
    qsort(out, used, sizeof(*out), descriptor_compare);
    *out_count = used;
    manager_unlock(m);
    return 1;
}
