/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef WINUX_WIN32_PLATFORM_H
#define WINUX_WIN32_PLATFORM_H

#include <stddef.h>
#include <stdint.h>

int winux_platform_memory_allocate(
    size_t bytes,
    void **out_address,
    uint32_t *out_platform_error
);

void winux_platform_memory_release(void *address);
uint32_t winux_platform_page_size(void);

#endif
