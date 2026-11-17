/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <winux/winux.h>

#include <cassert>
#include <iostream>

int main()
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

    std::cout << "Winux runtime lifecycle tests passed.\n";
    return 0;
}
