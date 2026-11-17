/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef WINUX_RUNTIME_CONTROLLER_HPP
#define WINUX_RUNTIME_CONTROLLER_HPP

#include <cstddef>

#include <winux/winux.h>

namespace winux {

class RuntimeController final {
public:
    RuntimeController() noexcept = default;
    ~RuntimeController();

    RuntimeController(const RuntimeController &) = delete;
    RuntimeController &operator=(const RuntimeController &) = delete;

    RuntimeController(RuntimeController &&other) noexcept;
    RuntimeController &operator=(RuntimeController &&other) noexcept;

    winux_status initialize(std::size_t memory_bytes = WINUX_DEFAULT_MEMORY_BYTES);
    winux_status start();
    winux_status stop();
    winux_status snapshot(winux_runtime_snapshot &out_snapshot) const noexcept;

private:
    void reset() noexcept;

    winux_runtime *runtime_ = nullptr;
};

} // namespace winux

#endif
