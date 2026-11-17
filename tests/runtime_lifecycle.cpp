/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifdef NDEBUG
#undef NDEBUG
#endif

#include <winux/winux.h>
#include "host_api.h"
#include "host_fault.h"
#include "fault_dispatcher.h"

#include <cassert>
#include <cstdint>
#include <iostream>

namespace {

uint32_t signal_event_from_thread(void *context, const winux_host_stop_token *stop_token)
{
    (void)stop_token;
    auto *event = static_cast<winux_host_event *>(context);
    uint32_t error = 0u;
    const int result = winux_host_event_set(event, &error);
    assert(result == 1);
    assert(error == 0u);
    return 0x5A5Au;
}


struct stop_test_context {
    winux_host_event *started;
    int observed_stop;
};

uint32_t wait_for_stop(void *raw_context, const winux_host_stop_token *stop_token)
{
    auto *context = static_cast<stop_test_context *>(raw_context);
    uint32_t error = 0u;
    (void)winux_host_event_set(context->started, &error);
    while (!winux_host_stop_requested(stop_token)) {
        /* Cooperative polling: no forced thread termination. */
    }
    context->observed_stop = 1;
    return 0x53544F50u;
}

void test_cooperative_thread_stop()
{
    winux_host_event *started = nullptr;
    winux_host_thread *thread = nullptr;
    uint32_t error = 0u;
    uint32_t exit_code = 0u;
    stop_test_context context{};

    assert(winux_host_event_create(1, 0, &started, &error) == 1);
    context.started = started;
    assert(winux_host_thread_create(wait_for_stop, &context, &thread, &error) == 1);
    assert(winux_host_event_wait(started, 5000u, &error) == WINUX_HOST_WAIT_SIGNALED);
    assert(winux_host_thread_request_stop(thread, &error) == 1);
    assert(winux_host_thread_join(thread, &exit_code, &error) == 1);
    assert(context.observed_stop == 1);
    assert(exit_code == 0x53544F50u);
    assert(winux_host_thread_request_stop(thread, &error) == 1);

    winux_host_thread_destroy(thread);
    winux_host_event_destroy(started);
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


void test_host_fault_boundary()
{
    const std::size_t page_size =
        static_cast<std::size_t>(winux_host_page_size());
    winux_host_memory_region region{};
    winux_host_fault_record fault{};
    uint32_t error = 0u;
    uint8_t value = 0u;

    assert(winux_host_memory_reserve(page_size, &region, &error) == 1);
    assert(winux_host_memory_commit(&region, 0u, page_size, &error) == 1);

    auto *bytes = static_cast<unsigned char *>(region.base);
    bytes[0] = 0xC3u;
    assert(winux_host_memory_probe_read(bytes, &value, &fault) == 1);
    assert(value == 0xC3u);
    assert(fault.kind == WINUX_HOST_FAULT_NONE);

    assert(winux_host_memory_protect(
        &region, 0u, page_size, WINUX_HOST_MEMORY_NO_ACCESS, &error) == 1);
    assert(winux_host_memory_probe_read(bytes, &value, &fault) == 0);
    assert(fault.kind == WINUX_HOST_FAULT_ACCESS_VIOLATION);
    assert(fault.access == WINUX_HOST_FAULT_ACCESS_READ);
    assert(fault.fault_address == reinterpret_cast<uintptr_t>(bytes));
    assert(fault.platform_exception_code == 0xC0000005u);

    winux_host_memory_release(&region);
}


void test_fault_dispatcher()
{
    const winux_memory_region_descriptor regions[] = {
        { 0x1000u, 0x100u, WINUX_MEMORY_REGION_COMMITTED | WINUX_MEMORY_REGION_READ },
        { 0x2000u, 0x100u, 0u },
        { 0x3000u, 0x100u, WINUX_MEMORY_REGION_COMMITTED |
          WINUX_MEMORY_REGION_READ | WINUX_MEMORY_REGION_GUARD }
    };
    winux_host_fault_record fault{};
    winux_fault_decision decision{};

    fault.kind = WINUX_HOST_FAULT_ACCESS_VIOLATION;
    fault.access = WINUX_HOST_FAULT_ACCESS_READ;
    fault.fault_address = 0x1010u;
    assert(winux_fault_dispatcher_classify(
        &fault, regions, 3u, &decision) == 1);
    assert(decision.disposition == WINUX_FAULT_DISPOSITION_UNEXPECTED_HOST_FAULT);
    assert(decision.region_index == 0u);

    fault.access = WINUX_HOST_FAULT_ACCESS_WRITE;
    assert(winux_fault_dispatcher_classify(
        &fault, regions, 3u, &decision) == 1);
    assert(decision.disposition == WINUX_FAULT_DISPOSITION_PROTECTION_VIOLATION);

    fault.access = WINUX_HOST_FAULT_ACCESS_READ;
    fault.fault_address = 0x2010u;
    assert(winux_fault_dispatcher_classify(
        &fault, regions, 3u, &decision) == 1);
    assert(decision.disposition == WINUX_FAULT_DISPOSITION_UNMAPPED);
    assert(decision.region_index == 1u);

    fault.fault_address = 0x4010u;
    assert(winux_fault_dispatcher_classify(
        &fault, regions, 3u, &decision) == 1);
    assert(decision.disposition == WINUX_FAULT_DISPOSITION_UNMAPPED);
    assert(decision.region_index == SIZE_MAX);

    fault.fault_address = 0x3010u;
    assert(winux_fault_dispatcher_classify(
        &fault, regions, 3u, &decision) == 1);
    assert(decision.disposition == WINUX_FAULT_DISPOSITION_GUARD_PAGE);

    fault.fault_address = 0x1010u;
    fault.access = WINUX_HOST_FAULT_ACCESS_UNKNOWN;
    assert(winux_fault_dispatcher_classify(
        &fault, regions, 3u, &decision) == 1);
    assert(decision.disposition == WINUX_FAULT_DISPOSITION_UNKNOWN_ACCESS);

    fault.kind = WINUX_HOST_FAULT_OTHER;
    assert(winux_fault_dispatcher_classify(
        &fault, regions, 3u, &decision) == 1);
    assert(decision.disposition == WINUX_FAULT_DISPOSITION_UNSUPPORTED);

    {
        const winux_memory_region_descriptor overlapping[] = {
            { 0x1000u, 0x100u, WINUX_MEMORY_REGION_COMMITTED },
            { 0x1080u, 0x100u, WINUX_MEMORY_REGION_COMMITTED }
        };
        assert(winux_fault_dispatcher_classify(
            &fault, overlapping, 2u, &decision) == 0);
    }

    assert(winux_fault_dispatcher_classify(
        nullptr, regions, 3u, &decision) == 0);
    assert(winux_fault_dispatcher_classify(
        &fault, regions, 3u, nullptr) == 0);
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
    test_cooperative_thread_stop();
    test_host_fault_boundary();
    test_fault_dispatcher();
    test_monotonic_clock();

    std::cout << "Winux host primitive tests passed.\n";
    return 0;
}
