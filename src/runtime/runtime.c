/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <stdlib.h>

#include <winux/winux.h>

#include "win32_platform.h"

struct winux_runtime {
    winux_runtime_state state;
    void *memory_arena;
    size_t memory_arena_bytes;
    uint32_t host_page_size_bytes;
    uint32_t last_platform_error;
};

void winux_config_init(winux_config *config)
{
    if (config == NULL) {
        return;
    }

    config->memory_limit_bytes = (size_t)WINUX_DEFAULT_MEMORY_BYTES;
}

winux_status winux_runtime_create(
    const winux_config *config,
    winux_runtime **out_runtime
)
{
    winux_config effective_config;
    winux_runtime *runtime;

    if (out_runtime == NULL) {
        return WINUX_STATUS_INVALID_ARGUMENT;
    }

    *out_runtime = NULL;
    winux_config_init(&effective_config);
    if (config != NULL) {
        effective_config = *config;
    }

    if (effective_config.memory_limit_bytes < (size_t)WINUX_MIN_MEMORY_BYTES ||
        effective_config.memory_limit_bytes > (size_t)WINUX_MAX_MEMORY_BYTES) {
        return WINUX_STATUS_INVALID_ARGUMENT;
    }

    runtime = (winux_runtime *)calloc(1u, sizeof(*runtime));
    if (runtime == NULL) {
        return WINUX_STATUS_OUT_OF_MEMORY;
    }

    if (!winux_platform_memory_allocate(
            effective_config.memory_limit_bytes,
            &runtime->memory_arena,
            &runtime->last_platform_error)) {
        free(runtime);
        return WINUX_STATUS_PLATFORM_ERROR;
    }

    runtime->state = WINUX_RUNTIME_STATE_CREATED;
    runtime->memory_arena_bytes = effective_config.memory_limit_bytes;
    runtime->host_page_size_bytes = winux_platform_page_size();
    *out_runtime = runtime;

    return WINUX_STATUS_OK;
}

winux_status winux_runtime_start(winux_runtime *runtime)
{
    if (runtime == NULL) {
        return WINUX_STATUS_INVALID_ARGUMENT;
    }

    if (runtime->state != WINUX_RUNTIME_STATE_CREATED) {
        return WINUX_STATUS_INVALID_STATE;
    }

    runtime->state = WINUX_RUNTIME_STATE_RUNNING;
    return WINUX_STATUS_OK;
}

winux_status winux_runtime_stop(winux_runtime *runtime)
{
    if (runtime == NULL) {
        return WINUX_STATUS_INVALID_ARGUMENT;
    }

    if (runtime->state != WINUX_RUNTIME_STATE_RUNNING) {
        return WINUX_STATUS_INVALID_STATE;
    }

    runtime->state = WINUX_RUNTIME_STATE_STOPPED;
    return WINUX_STATUS_OK;
}

winux_status winux_runtime_get_snapshot(
    const winux_runtime *runtime,
    winux_runtime_snapshot *out_snapshot
)
{
    if (runtime == NULL || out_snapshot == NULL) {
        return WINUX_STATUS_INVALID_ARGUMENT;
    }

    out_snapshot->state = runtime->state;
    out_snapshot->host_memory_arena_bytes = (uint64_t)runtime->memory_arena_bytes;
    out_snapshot->host_page_size_bytes = runtime->host_page_size_bytes;
    out_snapshot->last_platform_error = runtime->last_platform_error;

    return WINUX_STATUS_OK;
}

void winux_runtime_destroy(winux_runtime *runtime)
{
    if (runtime == NULL) {
        return;
    }

    winux_platform_memory_release(runtime->memory_arena);
    runtime->memory_arena = NULL;
    free(runtime);
}

const char *winux_status_string(winux_status status)
{
    switch (status) {
    case WINUX_STATUS_OK:
        return "success";
    case WINUX_STATUS_INVALID_ARGUMENT:
        return "invalid argument";
    case WINUX_STATUS_INVALID_STATE:
        return "invalid runtime state";
    case WINUX_STATUS_OUT_OF_MEMORY:
        return "out of memory";
    case WINUX_STATUS_PLATFORM_ERROR:
        return "Windows platform API failure";
    default:
        return "unknown status";
    }
}

const char *winux_runtime_state_string(winux_runtime_state state)
{
    switch (state) {
    case WINUX_RUNTIME_STATE_CREATED:
        return "created";
    case WINUX_RUNTIME_STATE_RUNNING:
        return "running";
    case WINUX_RUNTIME_STATE_STOPPED:
        return "stopped";
    default:
        return "unknown";
    }
}
