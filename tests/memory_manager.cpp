/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "host_api.h"
#include "memory_manager.h"
#include <cassert>
#include <vector>
int main() {
    const size_t page = static_cast<size_t>(winux_host_page_size());
    winux_memory_manager *manager = nullptr;
    winux_host_memory_region region{};
    size_t count = 0u;
    assert(page != 0u);
    assert(winux_memory_manager_create(&manager) == 1);
    assert(winux_memory_manager_reserve(manager, page * 3u, &region) == 1);
    assert(winux_memory_manager_snapshot(manager, nullptr, 0u, &count) == 0 && count == 1u);
    std::vector<winux_memory_region_descriptor> snapshot(count);
    assert(winux_memory_manager_snapshot(manager, snapshot.data(), snapshot.size(), &count) == 1);
    assert(count == 1u && snapshot[0].flags == 0u && snapshot[0].size == page * 3u);
    assert(winux_memory_manager_commit(manager, region.base, 0u, page * 2u) == 1);
    assert(winux_memory_manager_snapshot(manager, snapshot.data(), snapshot.size(), &count) == 0 && count == 2u);
    snapshot.resize(count);
    assert(winux_memory_manager_snapshot(manager, snapshot.data(), snapshot.size(), &count) == 1);
    assert(snapshot[0].flags == (WINUX_MEMORY_REGION_COMMITTED | WINUX_MEMORY_REGION_READ | WINUX_MEMORY_REGION_WRITE));
    assert(snapshot[0].size == page * 2u);
    assert(snapshot[1].flags == 0u && snapshot[1].size == page);
    assert(winux_memory_manager_protect(manager, region.base, page, page, WINUX_HOST_MEMORY_READ_ONLY) == 1);
    assert(winux_memory_manager_snapshot(manager, snapshot.data(), snapshot.size(), &count) == 0 && count == 3u);
    snapshot.resize(count);
    assert(winux_memory_manager_snapshot(manager, snapshot.data(), snapshot.size(), &count) == 1);
    assert(snapshot[0].flags == (WINUX_MEMORY_REGION_COMMITTED | WINUX_MEMORY_REGION_READ | WINUX_MEMORY_REGION_WRITE));
    assert(snapshot[1].flags == (WINUX_MEMORY_REGION_COMMITTED | WINUX_MEMORY_REGION_READ));
    assert(snapshot[2].flags == 0u);
    assert(winux_memory_manager_decommit(manager, region.base, 0u, page) == 1);
    assert(winux_memory_manager_snapshot(manager, snapshot.data(), snapshot.size(), &count) == 1);
    assert(count == 3u && snapshot[0].flags == 0u &&
        snapshot[1].flags == (WINUX_MEMORY_REGION_COMMITTED | WINUX_MEMORY_REGION_READ) &&
        snapshot[2].flags == 0u);
    assert(winux_memory_manager_commit(manager, region.base, 1u, page) == 0);
    assert(winux_memory_manager_protect(manager, region.base, page * 2u, page, WINUX_HOST_MEMORY_READ_ONLY) == 0);
    assert(winux_memory_manager_release(manager, region.base) == 1);
    assert(winux_memory_manager_snapshot(manager, nullptr, 0u, &count) == 1 && count == 0u);
    winux_memory_manager_destroy(manager);
    return 0;
}
