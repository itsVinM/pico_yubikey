#pragma once

#include "core/crc.hpp"
#include "core/slot.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

namespace yk::core {
    // Persistent slot record stored in flash
    namespace storage{
        constexpr std::uint32_t kMagic = 0x594B5352;  // "Y K S R"
        constexpr std::uint32_t kVersion = 1;
        constexpr std::uint32_t kRecordBytes = 128;   // one aligned flash slot
        constexpr std::uint32_t kRecords = 2;         // slot1, slot2

        struct Record {
            SlotConfig config;
            SlotState state;
        };

        // Serialize slot record into 'out'
        inline void serialize(
            std::span<std::uint8_t> out,
            const Record& rec, 
            std::uint32_t slot_index) noexcept {
                std::fill(out.begin(), out.end(), 0);
                const auto put32 = [&](std::size_t off, std::uint32_t v){
                    out[off ] = static_cast<std::uint8_t>(v );
                    out[off + 1] = static_cast<std::uint8_t>(v >> 8);
                    out[off + 2] = static_cast<std::uint8_t>(v >> 16);
                    out[off + 3] = static_cast<std::uint8_t>(v >> 24);
                };
                const auto put64 = [&](std::size_t off, std::uint64_t v){
                    for (std::size_t idx = 0; idx < 8; ++idx){
                        out[off + idx] = static_cast<std::uint8_t>(v >> (8 * idx));
                    }
                };

                put32(0, kMagic);
                put32(4, kVersion);
                put32(8, slot_index);
                out[12] = static_cast<std::uint8_t>(rec.config.mode);
                out[13] = rec.config.digits;
                put32(14, rec.config.period_secs);
                put32(18, rec.config.secret_len);
                for (std::size_t i = 0; i < kMaxSecretBytes; i++)
                    out[22 + i] = rec.config.secret[i];
                put32(22 + kMaxSecretBytes, rec.config.static_len);
                for (std::size_t i = 0; i < kMaxStaticBytes; i++)
                    out[26 + kMaxSecretBytes + i] =
                        static_cast<std::uint8_t>(rec.config.static_text[i]);
                put64(26 + kMaxSecretBytes + kMaxStaticBytes, rec.state.hotp_counter);

                const std::uint32_t crc = crc32_final(crc32(out.first(124)));
                put32(124, crc);

            }

            // Deserialize slot record from 'in'
            inline bool deserialize(
                std::span<const std::uint8_t> in,
                Record& rec,
                std::uint32_t slot_index) noexcept {
                if (in.size() < kRecordBytes) { return false; }
                const auto get32 = [&](std::size_t off) -> std::uint32_t {
                    return static_cast<std::uint32_t>(in[off]) |
                           (static_cast<std::uint32_t>(in[off + 1]) << 8) |
                           (static_cast<std::uint32_t>(in[off + 2]) << 16) |
                           (static_cast<std::uint32_t>(in[off + 3]) << 24);
                };

                const auto get64 = [&](std::size_t off) -> std::uint64_t {
                    std::uint64_t v = 0;
                    for (std::size_t idx = 0; idx < 8; ++idx){
                        v |= static_cast<std::uint64_t>(in[off + idx]) << (8 * idx);
                    }
                    return v;
                };

                if (get32(0) != kMagic) { return false; }
                if (get32(4) != kVersion) { return false; }
                if (get32(8) != slot_index) return false;
                
                const std::uint32_t store_crc = get32(124);
                if (crc32_final(crc32(in.first(124))) != store_crc) { return false; }

                rec = Record{};
                rec.config.mode = static_cast<SlotMode>(in[12]);
                rec.config.digits = in[13];
                rec.config.period_secs = get32(14);
                rec.config.secret_len = get32(18);
                if (rec.config.secret_len > kMaxSecretBytes) return false;
                for (std::size_t i = 0; i < kMaxSecretBytes; i++)
                    rec.config.secret[i] = in[22 + i];
                rec.config.static_len = get32(22 + kMaxSecretBytes);
                if (rec.config.static_len > kMaxStaticBytes) return false;
                for (std::size_t i = 0; i < kMaxStaticBytes; i++)
                    rec.config.static_text[i] =
                        static_cast<char>(in[26 + kMaxSecretBytes + i]);
                rec.state.hotp_counter = get64(26 + kMaxSecretBytes + kMaxStaticBytes);
                return true;
            }

            // Flash wear is erased to 0xFF; a record is absent when its magic is gone.
            inline bool is_absent(std::span<const std::uint8_t> in) noexcept {
                return in.size() < kRecordBytes || in[0] == 0xFF;
            }
        } // namespace storage

} // namespace yk::core