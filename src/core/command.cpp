#include "core/command.hpp"

#include "core/hmac.hpp"

#include <cstring>

namespace yk::core {
namespace cmd {
namespace {

std::size_t put_status(std::span<std::uint8_t> out, Status s) noexcept {
    if (out.empty()) return 0;
    out[0] = static_cast<std::uint8_t>(s);
    return 1;
}

std::size_t put_u64(std::span<std::uint8_t> out, std::size_t off,
                    std::uint64_t v) noexcept {
    for (std::size_t i = 0; i < 8; i++)
        out[off + i] = static_cast<std::uint8_t>(v >> (8 * i));
    return 8;
}

std::uint64_t get_u64(std::span<const std::uint8_t> in, std::size_t off) noexcept {
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < 8; i++)
        v |= std::uint64_t{in[off + i]} << (8 * i);
    return v;
}

std::uint32_t get_u32(std::span<const std::uint8_t> in, std::size_t off) noexcept {
    return std::uint32_t{in[off]} | (std::uint32_t{in[off + 1]} << 8) |
           (std::uint32_t{in[off + 2]} << 16) | (std::uint32_t{in[off + 3]} << 24);
}

bool slot_ok(std::uint32_t slot) noexcept {
    return slot < static_cast<std::uint32_t>(Slot::count);
}

} // namespace

std::size_t dispatch(Session& s, std::span<const std::uint8_t> in,
                     std::span<std::uint8_t> out,
                     std::uint64_t epoch_now_secs) noexcept {
    if (in.empty()) return put_status(out, Status::bad_command);

    switch (static_cast<CmdId>(in[0])) {
        case CmdId::get_status: {
            if (out.size() < 4) return put_status(out, Status::bad_param);
            out[0] = static_cast<std::uint8_t>(Status::ok);
            out[1] = static_cast<std::uint8_t>(Slot::count);
            for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(Slot::count); i++)
                out[2 + i] = static_cast<std::uint8_t>(s.slots[i].mode);
            return 2 + static_cast<std::uint32_t>(Slot::count);
        }

        case CmdId::set_slot: {
            if (in.size() < 9) return put_status(out, Status::bad_param);
            const std::uint32_t slot = in[1];
            if (!slot_ok(slot)) return put_status(out, Status::bad_slot);
            const std::uint32_t secret_len = in[8];
            if (secret_len > kMaxSecretBytes || in.size() < 9u + secret_len)
                return put_status(out, Status::bad_param);

            SlotConfig cfg{};
            cfg.mode = static_cast<SlotMode>(in[2]);
            cfg.digits = in[3];
            cfg.period_secs = get_u32(in, 4);
            cfg.secret_len = secret_len;
            for (std::uint32_t i = 0; i < secret_len; i++)
                cfg.secret[i] = in[9 + i];

            s.slots[slot] = cfg;
            s.states[slot].hotp_counter = 0;
            s.config_dirty = true;
            return put_status(out, Status::ok);
        }

        case CmdId::clear_slot: {
            if (in.size() < 2) return put_status(out, Status::bad_param);
            const std::uint32_t slot = in[1];
            if (!slot_ok(slot)) return put_status(out, Status::bad_slot);
            s.slots[slot] = SlotConfig{};
            s.states[slot].hotp_counter = 0;
            s.config_dirty = true;
            return put_status(out, Status::ok);
        }

        case CmdId::get_slot: {
            if (in.size() < 2) return put_status(out, Status::bad_param);
            const std::uint32_t slot = in[1];
            if (!slot_ok(slot)) return put_status(out, Status::bad_slot);
            const SlotConfig& cfg = s.slots[slot];
            if (out.size() < 6u + cfg.secret_len)
                return put_status(out, Status::bad_param);
            out[0] = static_cast<std::uint8_t>(Status::ok);
            out[1] = static_cast<std::uint8_t>(cfg.mode);
            out[2] = cfg.digits;
            out[3] = static_cast<std::uint8_t>(cfg.period_secs);
            out[4] = static_cast<std::uint8_t>(cfg.period_secs >> 8);
            out[5] = static_cast<std::uint8_t>(cfg.period_secs >> 16);
            out[6] = static_cast<std::uint8_t>(cfg.period_secs >> 24);
            for (std::uint32_t i = 0; i < cfg.secret_len; i++)
                out[7 + i] = cfg.secret[i];
            return 7u + cfg.secret_len;
        }

        case CmdId::challenge: {
            if (in.size() < 3) return put_status(out, Status::bad_param);
            const std::uint32_t slot = in[1];
            if (!slot_ok(slot)) return put_status(out, Status::bad_slot);
            const std::uint32_t challenge_len = in[2];
            if (challenge_len > kMaxChallengeBytes || in.size() < 3u + challenge_len)
                return put_status(out, Status::bad_param);
            if (s.slots[slot].mode != SlotMode::challenge)
                return put_status(out, Status::busy);

            s.challenge_slot = slot;
            s.challenge_len = challenge_len;
            for (std::uint32_t i = 0; i < challenge_len; i++)
                s.challenge[i] = in[3 + i];

            const auto mac = hmac_sha1(
                std::span<const std::uint8_t>(s.slots[slot].secret).first(
                    s.slots[slot].secret_len),
                std::span<const std::uint8_t>(s.challenge).first(challenge_len));
            if (out.size() < 1u + mac.size())
                return put_status(out, Status::bad_param);
            out[0] = static_cast<std::uint8_t>(Status::ok);
            std::memcpy(out.data() + 1, mac.data(), mac.size());
            return 1u + mac.size();
        }

        case CmdId::set_time: {
            if (in.size() < 9) return put_status(out, Status::bad_param);
            const std::uint64_t target = get_u64(in, 1);
            s.time_offset_secs = target - epoch_now_secs;
            return put_status(out, Status::ok);
        }

        case CmdId::get_time: {
            if (out.size() < 9) return put_status(out, Status::bad_param);
            out[0] = static_cast<std::uint8_t>(Status::ok);
            put_u64(out, 1, epoch_now_secs);
            return 9;
        }

        case CmdId::set_static: {
            if (in.size() < 3) return put_status(out, Status::bad_param);
            const std::uint32_t slot = in[1];
            if (!slot_ok(slot)) return put_status(out, Status::bad_slot);
            const std::uint32_t text_len = in[2];
            if (text_len > kMaxStaticBytes || in.size() < 3u + text_len)
                return put_status(out, Status::bad_param);
            s.slots[slot].static_len = text_len;
            for (std::uint32_t i = 0; i < text_len; i++)
                s.slots[slot].static_text[i] = static_cast<char>(in[3 + i]);
            s.config_dirty = true;
            return put_status(out, Status::ok);
        }

        default:
            return put_status(out, Status::bad_command);
    }
}

} // namespace cmd
} // namespace yk::core
