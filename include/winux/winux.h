/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Winux public C API.
 */
#ifndef WINUX_WINUX_H
#define WINUX_WINUX_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WINUX_VERSION_MAJOR 0
#define WINUX_VERSION_MINOR 1
#define WINUX_VERSION_PATCH 0

#define WINUX_MIB (1024u * 1024u)
#define WINUX_DEFAULT_MEMORY_BYTES (64u * WINUX_MIB)
#define WINUX_MIN_MEMORY_BYTES (1u * WINUX_MIB)
#define WINUX_MAX_MEMORY_BYTES (1024u * WINUX_MIB)

typedef struct winux_runtime winux_runtime;

typedef enum winux_status {
    WINUX_STATUS_OK = 0,
    WINUX_STATUS_INVALID_ARGUMENT,
    WINUX_STATUS_INVALID_STATE,
    WINUX_STATUS_OUT_OF_MEMORY,
    WINUX_STATUS_PLATFORM_ERROR
} winux_status;

typedef enum winux_runtime_state {
    WINUX_RUNTIME_STATE_CREATED = 0,
    WINUX_RUNTIME_STATE_RUNNING,
    WINUX_RUNTIME_STATE_STOPPED
} winux_runtime_state;

typedef struct winux_config {
    size_t memory_limit_bytes;
} winux_config;

typedef struct winux_runtime_snapshot {
    winux_runtime_state state;
    uint64_t host_memory_arena_bytes;
    uint32_t host_page_size_bytes;
    uint32_t last_platform_error;
} winux_runtime_snapshot;

void winux_config_init(winux_config *config);

winux_status winux_runtime_create(
    const winux_config *config,
    winux_runtime **out_runtime
);

winux_status winux_runtime_start(winux_runtime *runtime);
winux_status winux_runtime_stop(winux_runtime *runtime);

winux_status winux_runtime_get_snapshot(
    const winux_runtime *runtime,
    winux_runtime_snapshot *out_snapshot
);

void winux_runtime_destroy(winux_runtime *runtime);

const char *winux_status_string(winux_status status);
const char *winux_runtime_state_string(winux_runtime_state state);

#ifdef __cplusplus
}
#endif

#endif
