/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Platform-neutral host contract for the experimental Winux runtime.
 */
#ifndef WINUX_HOST_API_H
#define WINUX_HOST_API_H

#include <stddef.h>
#include <stdint.h>

typedef struct winux_host_memory_region {
    void *base;
    size_t size;
} winux_host_memory_region;

typedef enum winux_host_memory_protection {
    WINUX_HOST_MEMORY_NO_ACCESS = 0,
    WINUX_HOST_MEMORY_READ_ONLY,
    WINUX_HOST_MEMORY_READ_WRITE
} winux_host_memory_protection;

typedef struct winux_host_thread winux_host_thread;
typedef struct winux_host_stop_token winux_host_stop_token;
typedef struct winux_host_event winux_host_event;
typedef struct winux_host_mutex winux_host_mutex;

/* Worker callbacks must poll the token and return cooperatively. */
typedef uint32_t (*winux_host_thread_entry_fn)(
    void *context,
    const winux_host_stop_token *stop_token
);

typedef enum winux_host_wait_result {
    WINUX_HOST_WAIT_FAILED = -1,
    WINUX_HOST_WAIT_TIMEOUT = 0,
    WINUX_HOST_WAIT_SIGNALED = 1
} winux_host_wait_result;

#ifdef __cplusplus
extern "C" {
#endif

int winux_host_memory_allocate(
    size_t bytes,
    void **out_address,
    uint32_t *out_platform_error
);
void winux_host_memory_free(void *address);
uint32_t winux_host_page_size(void);

int winux_host_memory_reserve(
    size_t bytes,
    winux_host_memory_region *out_region,
    uint32_t *out_platform_error
);
int winux_host_memory_commit(
    winux_host_memory_region *region,
    size_t offset,
    size_t bytes,
    uint32_t *out_platform_error
);
int winux_host_memory_decommit(
    winux_host_memory_region *region,
    size_t offset,
    size_t bytes,
    uint32_t *out_platform_error
);
int winux_host_memory_protect(
    winux_host_memory_region *region,
    size_t offset,
    size_t bytes,
    winux_host_memory_protection protection,
    uint32_t *out_platform_error
);
void winux_host_memory_release(winux_host_memory_region *region);

int winux_host_thread_create(
    winux_host_thread_entry_fn entry,
    void *context,
    winux_host_thread **out_thread,
    uint32_t *out_platform_error
);
int winux_host_thread_request_stop(
    winux_host_thread *thread,
    uint32_t *out_platform_error
);
int winux_host_stop_requested(const winux_host_stop_token *stop_token);
int winux_host_thread_join(
    winux_host_thread *thread,
    uint32_t *out_exit_code,
    uint32_t *out_platform_error
);
void winux_host_thread_destroy(winux_host_thread *thread);

int winux_host_event_create(
    int manual_reset,
    int initially_signaled,
    winux_host_event **out_event,
    uint32_t *out_platform_error
);
int winux_host_event_set(winux_host_event *event, uint32_t *out_platform_error);
int winux_host_event_reset(winux_host_event *event, uint32_t *out_platform_error);
winux_host_wait_result winux_host_event_wait(
    winux_host_event *event,
    uint32_t timeout_ms,
    uint32_t *out_platform_error
);
void winux_host_event_destroy(winux_host_event *event);

int winux_host_mutex_create(
    winux_host_mutex **out_mutex,
    uint32_t *out_platform_error
);
int winux_host_mutex_lock(winux_host_mutex *mutex, uint32_t *out_platform_error);
int winux_host_mutex_unlock(winux_host_mutex *mutex, uint32_t *out_platform_error);
void winux_host_mutex_destroy(winux_host_mutex *mutex);

uint64_t winux_host_monotonic_time_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* WINUX_HOST_API_H */
