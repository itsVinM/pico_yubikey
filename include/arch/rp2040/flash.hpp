#pragma once

#include "core/command.hpp"
#include "core/storage.hpp"

#include <cstdint>

namespace yk::rp2040 {
    // Persistent slot storage on the RP2040's on-chip flash (2 MB). Config lives
    // in the final 64 KiB of flash so it can never collide with the firmware.
    //
    // Physical layout: one flash page (256 B) per slot at a fixed XIP address.
    // The pico-sdk flash API requires 256-byte alignment and page-sized writes,
    // which the 128-byte record + 128-byte slack satisfies.

    class FlashStore {
    public:
        // Read both slots from flash
        void load(yk::core::cmd::Session& session) const noexcept;
        bool save(const yk::core::cmd::Session& session) const noexcept;
    private:
        void load_one(yk::core::cmd::Session& session, std::uint32_t slot_index) const noexcept;
    };

    inline constexpr std::uint32_t kFlashDataOffset = 0x1F0000;  // last 64 KiB
    inline constexpr std::uint32_t kFlashDataXip = 0x10000000u + kFlashDataOffset;
    // One flash page (256 B) holds both 128-byte records. The pico-sdk flash API
    // requires 256-byte-aligned, page-sized writes.
    inline constexpr std::uint32_t kRecordSlotBytes =
        (yk::core::storage::kRecordBytes + 127) & ~127u;  // 128 per slot
    inline constexpr std::uint32_t kPageBytes = 256;
}// namespace yk::rp2040
