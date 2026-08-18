#pragma once

#include "core/slot.hpp"

#include <array>
#include <cstdint>
#include <span>

namespace yk::core {

    // Config protocol between host and device.
    //
    // Frames are binary, no length prefix: the transport (HID/CDC) is
    // message-oriented and knows the frame boundary. Frame layout:
    //   [0]     command id
    //   [1..]   command-specific payload
    //   (resp)  [0] status, [1..] payload
    //
    // The dispatch function is pure (host-testable): all device state lives in
    // Session and all time/IO is injected as parameters.
    namespace cmd {

        enum class CmdId : std::uint8_t {
            get_status = 1,
            set_slot = 2,
            clear_slot = 3,
            get_slot = 4,
            challenge = 5,
            set_time = 6,
            get_time = 7,
            set_static = 8,
        };

        enum class Status : std::uint8_t {
            ok = 0,
            bad_command = 1,
            bad_slot = 2,
            bad_param = 3,
            busy = 4,
            storage = 5,
        };

        // ASCII EOT used to delimit frames on the CDC transport.
        inline constexpr std::uint8_t kFrameEnd = 0x04;

        struct Session {
            std::array<SlotConfig, 2> slots{};
            std::array<SlotState, 2> states{};
            std::uint64_t time_offset_secs = 0;  // epoch = offset + device uptime
            std::array<std::uint8_t, kMaxChallengeBytes> challenge{};
            std::uint32_t challenge_len = 0;
            std::uint32_t challenge_slot = 0;
            // Set by dispatch when a command changed persistent config; the device
            // layer clears it after saving to flash.
            bool config_dirty = false;
        };

        // Execute one command frame. Writes the response into `out` (>= 16 bytes
        // recommended), returns bytes written. `epoch_now_secs` is the current
        // absolute time as seen by the device (offset + uptime).
        [[nodiscard]] std::size_t dispatch(Session& s, std::span<const std::uint8_t> in,
                                        std::span<std::uint8_t> out,
                                        std::uint64_t epoch_now_secs) noexcept;

        // Convenience accessors used by the device layer.
        inline std::uint64_t epoch_secs(const Session& s,
                                        std::uint64_t uptime_secs) noexcept {
            return s.time_offset_secs + uptime_secs;
        }

        } // namespace cmd


} // namespace yk
