#pragma once

#include <cstdint>
#include <algorithm>
#include <array>
#include <span>

namespace yk::core {
    // Bit-reduced CRC-32 (reflected polynomial 0xEDB88320) for one byte value.
    consteval std::uint32_t crc32_byte(std::uint32_t idx){
        std::uint32_t crc = idx;
        for (std::uint32_t key = 0; key < 8; key ++){
            crc = (crc & 1) ? (0xEDB88320 ^ (crc >> 1)) : (crc >> 1);
        }
        return crc;
    }
    
    // 256-entry lookup table for CRC-32
    consteval auto make_crc32_table(){
        std::array<std::uint32_t, 256> table{};
        for (std::uint32_t i = 0; i < 256; i++)
            table[i] = crc32_byte(i);
        return table;
    }

    inline constexpr auto kCrc32Table = make_crc32_table();

    // Table-driven CRC-32 calculation
    constexpr std::uint32_t crc32(std::span<const std::uint8_t> data, std::uint32_t crc = 0xFFFFFFFFu) {
        for (std::uint8_t b : data)
            crc = kCrc32Table[(crc ^ b) & 0xFFu] ^ (crc >> 8);
        return crc ^ 0xFFFFFFFFu;
    }

    // Already finalized by crc32(); present for API compatibility.
    inline std::uint32_t crc32_final(std::uint32_t crc) noexcept {
        return crc;
    }
} // namespace yk::core