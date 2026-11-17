/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Native Windows host primitives. This layer deliberately does not emulate
 * Linux physical memory, devices, or kernel execution.
 */
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "win32_platform.h"

int winux_platform_memory_allocate(
    size_t bytes,
    void **out_address,
    uint32_t *out_platform_error
)
{
    void *address;

    if (out_address == NULL || bytes == 0u) {
        return 0;
    }

    *out_address = NULL;
    if (out_platform_error != NULL) {
        *out_platform_error = ERROR_SUCCESS;
    }

    address = VirtualAlloc(NULL, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (address == NULL) {
        if (out_platform_error != NULL) {
            *out_platform_error = (uint32_t)GetLastError();
        }
        return 0;
    }

    *out_address = address;
    return 1;
}

void winux_platform_memory_release(void *address)
{
    if (address != NULL) {
        (void)VirtualFree(address, 0u, MEM_RELEASE);
    }
}

uint32_t winux_platform_page_size(void)
{
    SYSTEM_INFO system_info;

    GetSystemInfo(&system_info);
    return (uint32_t)system_info.dwPageSize;
}
