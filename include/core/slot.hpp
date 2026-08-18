#pragma once

#include "core/otp.hpp"
#include <array>
#include <cstdint>
#include <span>

namespace yk::core {
    // Slot model
    enum class SlotMode : std::uint8_t{
        unset = 0,        // empty, does nothing
        hotp = 1,         // one press = next HOTP code (counter advances, persisted)
        totp = 2,         // one press = TOTP code for current time window
        challenge = 3,    // challenge-response (HMAC-SHA1); triggered via config
        static_password = 4,  // types a fixed string
    };

    // Slot identifier on button : slot 1 = short press, slot 2 = long press, slot 3 = repeat press
    enum class Slot : std::uint8_t {slot1 = 0, slot2 = 1, count =  2};

    constexpr std::uint32_t kMaxSecretBytes = 32;
    constexpr std::uint32_t kMaxStaticBytes = 32;
    constexpr std::uint32_t kMaxChallengeBytes = 64;

    struct SlotConfig {
        SlotMode mode = SlotMode::unset;
        std::uint8_t digits = 6;          // OTP code width (6-8)
        std::uint32_t period_secs = 30;   // TOTP time step
        std::array<std::uint8_t, kMaxSecretBytes> secret{};
        std::uint32_t secret_len = 0;
        std::array<char, kMaxStaticBytes> static_text{};
        std::uint32_t static_len = 0;
    };

    // Per-slot mutable state
    struct SlotState {
        std::uint64_t hotp_counter = 0;  // HOTP counter (persisted)
    };

    // Print out
    struct TypedText {
        std::array<char, kMaxStaticBytes> text{};  // digits or static string
        std::uint32_t len = 0;    
    };

    // Button press event
    [[nodiscard]] inline TypedText execute_slot(
        const SlotConfig& config,
        SlotState& state,
        std::uint64_t unix_secs) noexcept{
        
        TypedText out;
        switch (config.mode) {
            case SlotMode::hotp: {
                auto code = otp::hotp(config.secret, state.hotp_counter, config.digits);
                state.hotp_counter++;  // consume the code; monotonic, persisted by caller
                out.len = static_cast<std::uint32_t>(config.digits);
                for (std::uint32_t i = 0; i < config.digits; i++) {
                    out.text[config.digits - 1 - i] = static_cast<char>('0' + code % 10);
                    code = code / 10;
                }
                break;
            }
            case SlotMode::totp: {
                auto code =
                    otp::totp(config.secret, unix_secs, config.period_secs, config.digits);
                out.len = static_cast<std::uint32_t>(config.digits);
                for (std::uint32_t i = 0; i < config.digits; i++) {
                    out.text[config.digits - 1 - i] = static_cast<char>('0' + code % 10);
                    code = code / 10;
                }
                break;
            }
            case SlotMode::static_password:
                out.len = config.static_len;
                for (std::uint32_t i = 0; i < config.static_len; i++)
                    out.text[i] = config.static_text[i];
                break;
            case SlotMode::challenge:
            case SlotMode::unset:
            default:
                break;  // nothing to type
        }
        return out;
        }


}//namespace yk::core