#pragma once

#include "core/hmac.hpp"

#include <array>
#include <cstdint>
#include <span>

namespace yk::core{
    // One-time password (OTP) generation using HMAC-SHA1 as specified in RFC 4226.
    namespace otp {
        constexpr std::uint8_t kMaxDigits = 8;

        // 8-byte big-endian moving factor block fed to HMAC
        inline std::array<std::uint8_t, 8> counter_block(std::uint64_t counter) noexcept {
            std::array<std::uint8_t, 8> out {};
            for (int idx = 7; idx >= 0; idx--){
                out[idx] = static_cast<std::uint8_t>(counter & 0xFF);
                counter >>= 8;
            }
            return out;
        }

        // RFC 4226 dynamic truncation - extract 31-bit value from HMAC digest
        inline std::uint32_t truncate(const Sha1::Digest& digest) noexcept {
            // Implementation for dynamic truncation
            const std::uint8_t offset = digest[19] & 0x0F;
            const std::uint32_t binary_code = (static_cast<std::uint32_t>(digest[offset]) & 0x7F) << 24 |
                                             (static_cast<std::uint32_t>(digest[offset + 1]) << 16) |
                                             (static_cast<std::uint32_t>(digest[offset + 2]) << 8) |
                                             (static_cast<std::uint32_t>(digest[offset + 3]));
            return binary_code;
        }

        inline std::uint32_t mod10(std::uint8_t digits) noexcept {
            std::uint32_t mod = 1;
            for (std::uint8_t i = 0; i < digits; ++i) {
                mod *= 10;      
            }
            return mod;
        }

        // HOTP (RFC 4226): code = (truncate(HMAC(key, counter)) mod 10^digits).
        inline std::uint32_t hotp(std::span<const std::uint8_t> key,
                                 std::uint64_t counter,
                                 std::uint8_t digits = 6) noexcept {
            if (digits > kMaxDigits) { digits = kMaxDigits; } // Limit to 8 digits
    
            const auto counter_bytes = counter_block(counter);
            const auto hmac_digest = hmac_sha1(key, counter_bytes);
            const auto binary_code = truncate(hmac_digest);
            return binary_code % mod10(digits);
        }

        //TOPT (RFC 6238):  HOTP with counter = floor(unix_time / period).
        inline std::uint32_t totp(std::span<const std::uint8_t> key,
                                 std::uint64_t unix_seconds,
                                 std::uint64_t period_seconds = 30,
                                 std::uint8_t digits = 6) noexcept {
            const auto counter = unix_seconds / period_seconds;
            return hotp(key, counter, digits);
        }
    } // namespace otp
} // namespace yk::core