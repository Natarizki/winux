/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "runtime_controller.hpp"

namespace winux {

RuntimeController::~RuntimeController()
{
    reset();
}

RuntimeController::RuntimeController(RuntimeController &&other) noexcept
    : runtime_(other.runtime_)
{
    other.runtime_ = nullptr;
}

RuntimeController &RuntimeController::operator=(RuntimeController &&other) noexcept
{
    if (this != &other) {
        reset();
        runtime_ = other.runtime_;
        other.runtime_ = nullptr;
    }
    return *this;
}

winux_status RuntimeController::initialize(std::size_t memory_bytes)
{
    if (runtime_ != nullptr) {
        return WINUX_STATUS_INVALID_STATE;
    }

    winux_config config;
    winux_config_init(&config);
    config.memory_limit_bytes = memory_bytes;

    return winux_runtime_create(&config, &runtime_);
}

winux_status RuntimeController::start()
{
    return winux_runtime_start(runtime_);
}

winux_status RuntimeController::stop()
{
    return winux_runtime_stop(runtime_);
}

winux_status RuntimeController::snapshot(
    winux_runtime_snapshot &out_snapshot
) const noexcept
{
    return winux_runtime_get_snapshot(runtime_, &out_snapshot);
}

void RuntimeController::reset() noexcept
{
    winux_runtime_destroy(runtime_);
    runtime_ = nullptr;
}

} // namespace winux
