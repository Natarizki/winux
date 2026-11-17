/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Windows host primitives. These APIs provide process-local resources only;
 * they do not emulate Linux kernel objects, physical memory, or devices.
 */
#include <windows.h>

#include <stdlib.h>

#include "host_api.h"

struct winux_host_stop_token {
    volatile LONG requested;
};

struct winux_host_thread {
    HANDLE handle;
    struct winux_host_stop_token stop_token;
    int joined;
    uint32_t exit_code;
};

struct winux_host_event {
    HANDLE handle;
};

struct winux_host_mutex {
    HANDLE handle;
};

struct winux_thread_start_context {
    winux_host_thread_entry_fn entry;
    void *context;
    const winux_host_stop_token *stop_token;
};

static void winux_set_error(uint32_t *out_error, DWORD error)
{
    if (out_error != NULL) {
        *out_error = (uint32_t)error;
    }
}

static int winux_host_memory_range_valid(
    const winux_host_memory_region *region,
    size_t offset,
    size_t bytes
)
{
    const size_t page_size = (size_t)winux_host_page_size();

    if (region == NULL || region->base == NULL || bytes == 0u || page_size == 0u) {
        return 0;
    }
    if ((offset % page_size) != 0u || (bytes % page_size) != 0u) {
        return 0;
    }
    if (offset > region->size || bytes > (region->size - offset)) {
        return 0;
    }
    return 1;
}

int winux_host_memory_allocate(
    size_t bytes,
    void **out_address,
    uint32_t *out_platform_error
)
{
    void *address;

    if (out_address == NULL || bytes == 0u) {
        winux_set_error(out_platform_error, ERROR_INVALID_PARAMETER);
        return 0;
    }

    *out_address = NULL;
    winux_set_error(out_platform_error, ERROR_SUCCESS);

    address = VirtualAlloc(NULL, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (address == NULL) {
        winux_set_error(out_platform_error, GetLastError());
        return 0;
    }

    *out_address = address;
    return 1;
}

void winux_host_memory_free(void *address)
{
    if (address != NULL) {
        (void)VirtualFree(address, 0u, MEM_RELEASE);
    }
}

uint32_t winux_host_page_size(void)
{
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    return (uint32_t)system_info.dwPageSize;
}

int winux_host_memory_reserve(
    size_t bytes,
    winux_host_memory_region *out_region,
    uint32_t *out_platform_error
)
{
    void *base;

    if (out_region == NULL || bytes == 0u) {
        winux_set_error(out_platform_error, ERROR_INVALID_PARAMETER);
        return 0;
    }

    out_region->base = NULL;
    out_region->size = 0u;
    winux_set_error(out_platform_error, ERROR_SUCCESS);

    base = VirtualAlloc(NULL, bytes, MEM_RESERVE, PAGE_NOACCESS);
    if (base == NULL) {
        winux_set_error(out_platform_error, GetLastError());
        return 0;
    }

    out_region->base = base;
    out_region->size = bytes;
    return 1;
}

int winux_host_memory_commit(
    winux_host_memory_region *region,
    size_t offset,
    size_t bytes,
    uint32_t *out_platform_error
)
{
    void *address;

    if (!winux_host_memory_range_valid(region, offset, bytes)) {
        winux_set_error(out_platform_error, ERROR_INVALID_PARAMETER);
        return 0;
    }

    address = (unsigned char *)region->base + offset;
    if (VirtualAlloc(address, bytes, MEM_COMMIT, PAGE_READWRITE) == NULL) {
        winux_set_error(out_platform_error, GetLastError());
        return 0;
    }

    winux_set_error(out_platform_error, ERROR_SUCCESS);
    return 1;
}

int winux_host_memory_decommit(
    winux_host_memory_region *region,
    size_t offset,
    size_t bytes,
    uint32_t *out_platform_error
)
{
    void *address;

    if (!winux_host_memory_range_valid(region, offset, bytes)) {
        winux_set_error(out_platform_error, ERROR_INVALID_PARAMETER);
        return 0;
    }

    address = (unsigned char *)region->base + offset;
    if (!VirtualFree(address, bytes, MEM_DECOMMIT)) {
        winux_set_error(out_platform_error, GetLastError());
        return 0;
    }

    winux_set_error(out_platform_error, ERROR_SUCCESS);
    return 1;
}

int winux_host_memory_protect(
    winux_host_memory_region *region,
    size_t offset,
    size_t bytes,
    winux_host_memory_protection protection,
    uint32_t *out_platform_error
)
{
    DWORD native_protection;
    DWORD old_protection;
    void *address;

    if (!winux_host_memory_range_valid(region, offset, bytes)) {
        winux_set_error(out_platform_error, ERROR_INVALID_PARAMETER);
        return 0;
    }

    switch (protection) {
    case WINUX_HOST_MEMORY_NO_ACCESS:
        native_protection = PAGE_NOACCESS;
        break;
    case WINUX_HOST_MEMORY_READ_ONLY:
        native_protection = PAGE_READONLY;
        break;
    case WINUX_HOST_MEMORY_READ_WRITE:
        native_protection = PAGE_READWRITE;
        break;
    default:
        winux_set_error(out_platform_error, ERROR_INVALID_PARAMETER);
        return 0;
    }

    address = (unsigned char *)region->base + offset;
    if (!VirtualProtect(address, bytes, native_protection, &old_protection)) {
        winux_set_error(out_platform_error, GetLastError());
        return 0;
    }

    winux_set_error(out_platform_error, ERROR_SUCCESS);
    return 1;
}

int winux_host_memory_release(winux_host_memory_region *region, uint32_t *out_platform_error)
{
    if (region == NULL || region->base == NULL) {
        winux_set_error(out_platform_error, ERROR_INVALID_PARAMETER);
        return 0;
    }
    if (!VirtualFree(region->base, 0u, MEM_RELEASE)) {
        winux_set_error(out_platform_error, GetLastError());
        return 0;
    }
    region->base = NULL;
    region->size = 0u;
    winux_set_error(out_platform_error, ERROR_SUCCESS);
    return 1;
}

static DWORD WINAPI winux_host_thread_entry(LPVOID raw_context)
{
    struct winux_thread_start_context *start =
        (struct winux_thread_start_context *)raw_context;
    winux_host_thread_entry_fn entry = start->entry;
    void *context = start->context;
    const winux_host_stop_token *stop_token = start->stop_token;
    uint32_t result;

    free(start);
    result = entry(context, stop_token);
    return (DWORD)result;
}

int winux_host_thread_create(
    winux_host_thread_entry_fn entry,
    void *context,
    winux_host_thread **out_thread,
    uint32_t *out_platform_error
)
{
    struct winux_thread_start_context *start;
    winux_host_thread *thread;
    HANDLE handle;

    if (entry == NULL || out_thread == NULL) {
        winux_set_error(out_platform_error, ERROR_INVALID_PARAMETER);
        return 0;
    }

    *out_thread = NULL;
    winux_set_error(out_platform_error, ERROR_SUCCESS);

    start = (struct winux_thread_start_context *)malloc(sizeof(*start));
    thread = (winux_host_thread *)calloc(1u, sizeof(*thread));
    if (start == NULL || thread == NULL) {
        free(start);
        free(thread);
        winux_set_error(out_platform_error, ERROR_NOT_ENOUGH_MEMORY);
        return 0;
    }

    start->entry = entry;
    start->context = context;
    start->stop_token = &thread->stop_token;

    handle = CreateThread(NULL, 0u, winux_host_thread_entry, start, 0u, NULL);
    if (handle == NULL) {
        winux_set_error(out_platform_error, GetLastError());
        free(start);
        free(thread);
        return 0;
    }

    thread->handle = handle;
    *out_thread = thread;
    return 1;
}

int winux_host_thread_request_stop(
    winux_host_thread *thread,
    uint32_t *out_platform_error
)
{
    if (thread == NULL) {
        winux_set_error(out_platform_error, ERROR_INVALID_PARAMETER);
        return 0;
    }

    (void)InterlockedExchange(&thread->stop_token.requested, 1);
    winux_set_error(out_platform_error, ERROR_SUCCESS);
    return 1;
}

int winux_host_stop_requested(const winux_host_stop_token *stop_token)
{
    if (stop_token == NULL) {
        return 0;
    }
    return InterlockedCompareExchange(
        (volatile LONG *)&stop_token->requested, 0, 0) != 0;
}

int winux_host_thread_join(
    winux_host_thread *thread,
    uint32_t *out_exit_code,
    uint32_t *out_platform_error
)
{
    DWORD wait_result;
    DWORD exit_code;

    if (thread == NULL) {
        winux_set_error(out_platform_error, ERROR_INVALID_PARAMETER);
        return 0;
    }

    if (!thread->joined) {
        wait_result = WaitForSingleObject(thread->handle, INFINITE);
        if (wait_result != WAIT_OBJECT_0) {
            winux_set_error(out_platform_error, GetLastError());
            return 0;
        }

        if (!GetExitCodeThread(thread->handle, &exit_code)) {
            winux_set_error(out_platform_error, GetLastError());
            return 0;
        }

        thread->exit_code = (uint32_t)exit_code;
        thread->joined = 1;
    }

    if (out_exit_code != NULL) {
        *out_exit_code = thread->exit_code;
    }
    winux_set_error(out_platform_error, ERROR_SUCCESS);
    return 1;
}

void winux_host_thread_destroy(winux_host_thread *thread)
{
    if (thread == NULL) {
        return;
    }

    if (!thread->joined) {
        (void)WaitForSingleObject(thread->handle, INFINITE);
    }
    (void)CloseHandle(thread->handle);
    free(thread);
}

int winux_host_event_create(
    int manual_reset,
    int initially_signaled,
    winux_host_event **out_event,
    uint32_t *out_platform_error
)
{
    winux_host_event *event;
    HANDLE handle;

    if (out_event == NULL) {
        winux_set_error(out_platform_error, ERROR_INVALID_PARAMETER);
        return 0;
    }

    *out_event = NULL;
    event = (winux_host_event *)calloc(1u, sizeof(*event));
    if (event == NULL) {
        winux_set_error(out_platform_error, ERROR_NOT_ENOUGH_MEMORY);
        return 0;
    }

    handle = CreateEventW(
        NULL,
        manual_reset ? TRUE : FALSE,
        initially_signaled ? TRUE : FALSE,
        NULL
    );
    if (handle == NULL) {
        winux_set_error(out_platform_error, GetLastError());
        free(event);
        return 0;
    }

    event->handle = handle;
    *out_event = event;
    winux_set_error(out_platform_error, ERROR_SUCCESS);
    return 1;
}

int winux_host_event_set(winux_host_event *event, uint32_t *out_platform_error)
{
    if (event == NULL) {
        winux_set_error(out_platform_error, ERROR_INVALID_PARAMETER);
        return 0;
    }
    if (!SetEvent(event->handle)) {
        winux_set_error(out_platform_error, GetLastError());
        return 0;
    }
    winux_set_error(out_platform_error, ERROR_SUCCESS);
    return 1;
}

int winux_host_event_reset(winux_host_event *event, uint32_t *out_platform_error)
{
    if (event == NULL) {
        winux_set_error(out_platform_error, ERROR_INVALID_PARAMETER);
        return 0;
    }
    if (!ResetEvent(event->handle)) {
        winux_set_error(out_platform_error, GetLastError());
        return 0;
    }
    winux_set_error(out_platform_error, ERROR_SUCCESS);
    return 1;
}

winux_host_wait_result winux_host_event_wait(
    winux_host_event *event,
    uint32_t timeout_ms,
    uint32_t *out_platform_error
)
{
    DWORD result;

    if (event == NULL) {
        winux_set_error(out_platform_error, ERROR_INVALID_PARAMETER);
        return WINUX_HOST_WAIT_FAILED;
    }

    result = WaitForSingleObject(event->handle, (DWORD)timeout_ms);
    if (result == WAIT_OBJECT_0) {
        winux_set_error(out_platform_error, ERROR_SUCCESS);
        return WINUX_HOST_WAIT_SIGNALED;
    }
    if (result == WAIT_TIMEOUT) {
        winux_set_error(out_platform_error, ERROR_SUCCESS);
        return WINUX_HOST_WAIT_TIMEOUT;
    }

    winux_set_error(out_platform_error, GetLastError());
    return WINUX_HOST_WAIT_FAILED;
}

void winux_host_event_destroy(winux_host_event *event)
{
    if (event == NULL) {
        return;
    }
    (void)CloseHandle(event->handle);
    free(event);
}

int winux_host_mutex_create(
    winux_host_mutex **out_mutex,
    uint32_t *out_platform_error
)
{
    winux_host_mutex *mutex;
    HANDLE handle;

    if (out_mutex == NULL) {
        winux_set_error(out_platform_error, ERROR_INVALID_PARAMETER);
        return 0;
    }

    *out_mutex = NULL;
    mutex = (winux_host_mutex *)calloc(1u, sizeof(*mutex));
    if (mutex == NULL) {
        winux_set_error(out_platform_error, ERROR_NOT_ENOUGH_MEMORY);
        return 0;
    }

    handle = CreateMutexW(NULL, FALSE, NULL);
    if (handle == NULL) {
        winux_set_error(out_platform_error, GetLastError());
        free(mutex);
        return 0;
    }

    mutex->handle = handle;
    *out_mutex = mutex;
    winux_set_error(out_platform_error, ERROR_SUCCESS);
    return 1;
}

int winux_host_mutex_lock(winux_host_mutex *mutex, uint32_t *out_platform_error)
{
    DWORD result;

    if (mutex == NULL) {
        winux_set_error(out_platform_error, ERROR_INVALID_PARAMETER);
        return 0;
    }

    result = WaitForSingleObject(mutex->handle, INFINITE);
    if (result != WAIT_OBJECT_0 && result != WAIT_ABANDONED) {
        winux_set_error(out_platform_error, GetLastError());
        return 0;
    }

    winux_set_error(out_platform_error, ERROR_SUCCESS);
    return 1;
}

int winux_host_mutex_unlock(winux_host_mutex *mutex, uint32_t *out_platform_error)
{
    if (mutex == NULL) {
        winux_set_error(out_platform_error, ERROR_INVALID_PARAMETER);
        return 0;
    }
    if (!ReleaseMutex(mutex->handle)) {
        winux_set_error(out_platform_error, GetLastError());
        return 0;
    }

    winux_set_error(out_platform_error, ERROR_SUCCESS);
    return 1;
}

void winux_host_mutex_destroy(winux_host_mutex *mutex)
{
    if (mutex == NULL) {
        return;
    }
    (void)CloseHandle(mutex->handle);
    free(mutex);
}

uint64_t winux_host_monotonic_time_ms(void)
{
    return (uint64_t)GetTickCount64();
}


#include "host_fault.h"

static int winux_host_fault_filter(
    EXCEPTION_POINTERS *exception_pointers,
    winux_host_fault_record *out_fault
)
{
    EXCEPTION_RECORD *record;
    ULONG_PTR operation;

    if (exception_pointers == NULL || exception_pointers->ExceptionRecord == NULL ||
        out_fault == NULL) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    record = exception_pointers->ExceptionRecord;
    if (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION &&
        record->ExceptionCode != STATUS_GUARD_PAGE_VIOLATION) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    out_fault->kind = record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION
        ? WINUX_HOST_FAULT_ACCESS_VIOLATION
        : WINUX_HOST_FAULT_GUARD_PAGE;
    out_fault->platform_exception_code = (uint32_t)record->ExceptionCode;
    out_fault->fault_address = (uintptr_t)record->ExceptionAddress;
    out_fault->access = WINUX_HOST_FAULT_ACCESS_UNKNOWN;

    if (record->NumberParameters >= 2u) {
        operation = record->ExceptionInformation[0];
        out_fault->fault_address = (uintptr_t)record->ExceptionInformation[1];
        if (operation == 0u) {
            out_fault->access = WINUX_HOST_FAULT_ACCESS_READ;
        } else if (operation == 1u) {
            out_fault->access = WINUX_HOST_FAULT_ACCESS_WRITE;
        } else if (operation == 8u) {
            out_fault->access = WINUX_HOST_FAULT_ACCESS_EXECUTE;
        }
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

int winux_host_memory_probe_read(
    const volatile void *address,
    uint8_t *out_value,
    winux_host_fault_record *out_fault
)
{
    if (address == NULL || out_value == NULL || out_fault == NULL) {
        return -1;
    }

    out_fault->kind = WINUX_HOST_FAULT_NONE;
    out_fault->access = WINUX_HOST_FAULT_ACCESS_UNKNOWN;
    out_fault->fault_address = 0u;
    out_fault->platform_exception_code = ERROR_SUCCESS;

    __try {
        *out_value = *(const volatile uint8_t *)address;
    }
    __except (winux_host_fault_filter(GetExceptionInformation(), out_fault)) {
        return 0;
    }

    return 1;
}
