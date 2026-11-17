/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "host_api.h"
#include "memory_manager.h"
#include "linux_vm_api.h"
#include <cassert>
#include <cstdint>

int main() {
    const size_t page = (size_t)winux_host_page_size();
    winux_memory_manager *memory = nullptr;
    winux_host_memory_region reservation{};
    winux_linux_vm_api *vm = nullptr;
    int error = 0;
    assert(page != 0);
    assert(winux_memory_manager_create(&memory));
    assert(winux_memory_manager_reserve(memory, page * 8, &reservation));
    assert(winux_linux_vm_api_create(memory, reservation.base,
                                    reservation.size, &vm));

    void *a = winux_linux_mmap(vm, nullptr, page + 1,
        WINUX_LINUX_PROT_READ | WINUX_LINUX_PROT_WRITE,
        WINUX_LINUX_MAP_PRIVATE | WINUX_LINUX_MAP_ANONYMOUS,
        -1, 0, &error);
    assert(a != WINUX_LINUX_MAP_FAILED && error == 0);
    assert((uintptr_t)a % page == 0);

    void *b = winux_linux_mmap(vm, nullptr, page,
        WINUX_LINUX_PROT_READ,
        WINUX_LINUX_MAP_PRIVATE | WINUX_LINUX_MAP_ANONYMOUS,
        -1, 0, &error);
    assert(b != WINUX_LINUX_MAP_FAILED && b == (void *)((uintptr_t)a + page * 2));

    assert(winux_linux_mprotect(vm, a, page, WINUX_LINUX_PROT_READ, &error) == 0);
    assert(winux_linux_mprotect(vm, a, page, WINUX_LINUX_PROT_WRITE, &error) == -1);
    assert(error == WINUX_LINUX_EINVAL);
    assert(winux_linux_mprotect(vm, a, page, WINUX_LINUX_PROT_EXEC, &error) == -1);
    assert(error == WINUX_LINUX_ENOTSUP);
    assert(winux_linux_munmap(vm, a, page * 2, &error) == 0);
    assert(winux_linux_munmap(vm, b, page, &error) == 0);

    assert(winux_linux_mmap(vm, nullptr, page, WINUX_LINUX_PROT_READ,
        WINUX_LINUX_MAP_SHARED | WINUX_LINUX_MAP_ANONYMOUS,
        -1, 0, &error) == WINUX_LINUX_MAP_FAILED);
    assert(error == WINUX_LINUX_EINVAL);

    winux_linux_vm_api_destroy(vm);
    assert(winux_memory_manager_release(memory, reservation.base));
    winux_memory_manager_destroy(memory);
    return 0;
}
