/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifdef NDEBUG
#undef NDEBUG
#endif

#include <winux/winux.h>
#include "host_api.h"

#include <cassert>
#include <cstdint>
#include <iostream>

namespace {

uint32_t signal_event_from_thread(void *context)
{
    auto *event = static_cast<winux_host_event *>(context);
    uint32_t error = 0u;
    const int result = winux_host_event_set(event, &error);
    assert(result == 1);
    assert(error == 0u);
    return 0x5A5Au;
}


void test_runtime_lifecycle()
{
    winux_config config{};
    winux_config_init(&config);
    config.memory_limit_bytes = 4u * 1024u * 1024u;

    winux_runtime *runtime = nullptr;
    assert(winux_runtime_create(&config, &runtime) == WINUX_STATUS_OK);
    assert(runtime != nullptr);

    winux_runtime_snapshot snapshot{};
    assert(winux_runtime_get_snapshot(runtime, &snapshot) == WINUX_STATUS_OK);
    assert(snapshot.state == WINUX_RUNTIME_STATE_CREATED);
    assert(snapshot.host_memory_arena_bytes == 4u * 1024u * 1024u);
    assert(snapshot.host_page_size_bytes > 0u);

    assert(winux_runtime_start(runtime) == WINUX_STATUS_OK);
    assert(winux_runtime_start(runtime) == WINUX_STATUS_INVALID_STATE);

    assert(winux_runtime_get_snapshot(runtime, &snapshot) == WINUX_STATUS_OK);
    assert(snapshot.state == WINUX_RUNTIME_STATE_RUNNING);

    assert(winux_runtime_stop(runtime) == WINUX_STATUS_OK);
    assert(winux_runtime_stop(runtime) == WINUX_STATUS_INVALID_STATE);

    assert(winux_runtime_get_snapshot(runtime, &snapshot) == WINUX_STATUS_OK);
    assert(snapshot.state == WINUX_RUNTIME_STATE_STOPPED);
    winux_runtime_destroy(runtime);

    runtime = nullptr;
    config.memory_limit_bytes = 0u;
    assert(winux_runtime_create(&config, &runtime) == WINUX_STATUS_INVALID_ARGUMENT);
    assert(runtime == nullptr);
    assert(winux_runtime_start(nullptr) == WINUX_STATUS_INVALID_ARGUMENT);
}

void test_virtual_memory()
{
    const std::size_t page_size =
        static_cast<std::size_t>(winux_host_page_size());
    winux_host_memory_region region{};
    uint32_t error = 0u;

    assert(page_size != 0u);
    assert(winux_host_memory_reserve(page_size * 2u, &region, &error) == 1);
    assert(region.base != nullptr);
    assert(region.size == page_size * 2u);

    assert(winux_host_memory_commit(&region, 0u, page_size, &error) == 1);
    auto *bytes = static_cast<unsigned char *>(region.base);
    bytes[0] = 0xA5u;
    bytes[page_size - 1u] = 0x5Au;
    assert(bytes[0] == 0xA5u);
    assert(bytes[page_size - 1u] == 0x5Au);

    assert(winux_host_memory_protect(
        &region, 0u, page_size, WINUX_HOST_MEMORY_READ_ONLY, &error) == 1);
    assert(bytes[0] == 0xA5u);
    assert(winux_host_memory_protect(
        &region, 0u, page_size, WINUX_HOST_MEMORY_READ_WRITE, &error) == 1);

    assert(winux_host_memory_commit(&region, 1u, page_size, &error) == 0);
    assert(winux_host_memory_decommit(&region, 0u, page_size, &error) == 1);
    winux_host_memory_release(&region);
    assert(region.base == nullptr);
    assert(region.size == 0u);
}

void test_thread_event_and_mutex()
{
    winux_host_event *event = nullptr;
    winux_host_thread *thread = nullptr;
    winux_host_mutex *mutex = nullptr;
    uint32_t error = 0u;
    uint32_t exit_code = 0u;

    assert(winux_host_event_create(1, 0, &event, &error) == 1);
    assert(winux_host_event_wait(event, 0u, &error) == WINUX_HOST_WAIT_TIMEOUT);

    assert(winux_host_thread_create(
        signal_event_from_thread, event, &thread, &error) == 1);
    assert(winux_host_thread_join(thread, &exit_code, &error) == 1);
    assert(exit_code == 0x5A5Au);
    assert(winux_host_event_wait(event, 5000u, &error) == WINUX_HOST_WAIT_SIGNALED);
    assert(winux_host_event_reset(event, &error) == 1);
    assert(winux_host_event_wait(event, 0u, &error) == WINUX_HOST_WAIT_TIMEOUT);

    winux_host_thread_destroy(thread);
    winux_host_event_destroy(event);

    assert(winux_host_mutex_create(&mutex, &error) == 1);
    assert(winux_host_mutex_lock(mutex, &error) == 1);
    assert(winux_host_mutex_unlock(mutex, &error) == 1);
    winux_host_mutex_destroy(mutex);
}

void test_monotonic_clock()
{
    const uint64_t before = winux_host_monotonic_time_ms();
    const uint64_t after = winux_host_monotonic_time_ms();
    assert(after >= before);
}

} // namespace

int main()
{
    test_runtime_lifecycle();
    test_virtual_memory();
    test_thread_event_and_mutex();
    test_monotonic_clock();

    std::cout << "Winux host primitive tests passed.\n";
    return 0;
}
