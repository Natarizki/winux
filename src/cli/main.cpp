/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "runtime_controller.hpp"

#include <cstdlib>
#include <iostream>

namespace {

constexpr std::size_t kPrototypeMemoryBytes = 64u * 1024u * 1024u;

int report_error(const char *operation, winux_status status)
{
    std::cerr << "Winux: " << operation << " failed: "
              << winux_status_string(status) << '\n';
    return EXIT_FAILURE;
}

} // namespace

int main()
{
    winux::RuntimeController runtime;

    std::cout << "Winux " << WINUX_VERSION_MAJOR << '.'
              << WINUX_VERSION_MINOR << '.' << WINUX_VERSION_PATCH
              << " - Windows host-runtime prototype\n";

    winux_status status = runtime.initialize(kPrototypeMemoryBytes);
    if (status != WINUX_STATUS_OK) {
        return report_error("initialize", status);
    }

    status = runtime.start();
    if (status != WINUX_STATUS_OK) {
        return report_error("start", status);
    }

    winux_runtime_snapshot snapshot{};
    status = runtime.snapshot(snapshot);
    if (status != WINUX_STATUS_OK) {
        return report_error("query runtime", status);
    }

    std::cout << "Host backend: Win32\n"
              << "Runtime state: "
              << winux_runtime_state_string(snapshot.state) << '\n'
              << "Host memory arena: "
              << (snapshot.host_memory_arena_bytes / (1024u * 1024u))
              << " MiB\n"
              << "Host page size: " << snapshot.host_page_size_bytes
              << " bytes\n"
              << "Linux kernel: not loaded (kernel-hosting backend pending)\n"
              << "Linux syscall ABI: not implemented\n";

    status = runtime.stop();
    if (status != WINUX_STATUS_OK) {
        return report_error("stop", status);
    }

    return EXIT_SUCCESS;
}
