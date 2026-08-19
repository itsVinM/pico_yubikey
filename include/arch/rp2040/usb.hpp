#pragma once

#include "core/command.hpp"
#include <cstdint>
#include <span>

namespace yk::rp2040 {

void usb_init() noexcept;
void usb_poll(cmd::Session& session, std::uint64_t epoch_now_secs) noexcept;
void usb_type_text(std::stan<const char> text) noexcept;

} // namespace yk::rp2040