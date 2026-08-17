#include "core/sha1.hpp"

#include <bit>
#include <cstring>

namespace yk::core {
namespace {
    constexpr std::uint32_t ivs[5] = {
        0x67452301,
        0xEFCDAB89,
        0x98BADCFE,
        0x10325476,
        0xC3D2E1F0
    };

    std::uint32_t load_be32(const std::uint8_t* data) noexcept {
        return (static_cast<std::uint32_t>(data[0]) << 24) |
               (static_cast<std::uint32_t>(data[1]) << 16) |
               (static_cast<std::uint32_t>(data[2]) << 8) |
               (static_cast<std::uint32_t>(data[3]));
    }

    void store_be32(std::uint8_t* data, std::uint32_t value) noexcept {
        data[0] = static_cast<std::uint8_t>(value >> 24);
        data[1] = static_cast<std::uint8_t>(value >> 16);
        data[2] = static_cast<std::uint8_t>(value >> 8);
        data[3] = static_cast<std::uint8_t>(value);
    }
} // namespace

    void Sha1::reset() noexcept {
        m_state = std::array<std::uint32_t, 5>{ivs[0], ivs[1], ivs[2], ivs[3], ivs[4]};
        m_buffer.fill(0);
        buffer_len_ = 0;
        total_len_ = 0;
    }

    void Sha1::compress(const std::array<std::uint8_t, kBlockSize>& block) noexcept {
        std::array<std::uint32_t, 80> w{};
        for (std::size_t i = 0; i < 16; ++i) {
            w[i] = load_be32(&block[i * 4]);
        }
        for (std::size_t i = 16; i < 80; ++i) {
            w[i] = std::rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }

        std::uint32_t a = m_state[0];
        std::uint32_t b = m_state[1];
        std::uint32_t c = m_state[2];
        std::uint32_t d = m_state[3];
        std::uint32_t e = m_state[4];

        for (std::size_t i = 0; i < 80; ++i) {
            std::uint32_t f, k;
            if (i < 20) {
                f = (b & c) | (~b & d);
                k = 0x5A827999;
            } else if (i < 40) {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            } else if (i < 60) {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            } else {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            std::uint32_t temp = std::rotl(a, 5) + f + e + k + w[i];
            e = d;
            d = c;
            c = std::rotl(b, 30);
            b = a;
            a = temp;
        }

        m_state[0] += a;
        m_state[1] += b;
        m_state[2] += c;
        m_state[3] += d;
        m_state[4] += e;
    }
    void Sha1::update(std::span<const std::uint8_t> data) noexcept {
        for (const uint8_t byte : data) {
            m_buffer[buffer_len_++] = byte;
            total_len_ ++;
            if (buffer_len_ == kBlockSize) {
                compress(m_buffer);
                buffer_len_ = 0;
            }
        }
    }

    Sha1::Digest Sha1::final() noexcept {
        const std::uint64_t bit_len = total_len_ * 8;

        m_buffer[buffer_len_++] = 0x80;
        // Pad with zeros until 8 bytes short of a block (room for the length).
        if (buffer_len_ > 56) {
            while (buffer_len_ < kBlockSize) m_buffer[buffer_len_++] = 0;
            compress(m_buffer);
            buffer_len_ = 0;
        }
        while (buffer_len_ < 56) m_buffer[buffer_len_++] = 0;

        // Append 64-bit big-endian bit length.
        for (int i = 0; i < 8; i++)
            m_buffer[56 + i] = static_cast<std::uint8_t>(bit_len >> (56 - 8 * i));
        buffer_len_ = kBlockSize;
        compress(m_buffer);
        buffer_len_ = 0;

        Digest out{};
        for (int i = 0; i < 5; i++) store_be32(out.data() + i * 4, m_state[i]);
        reset();
        return out;
}

Sha1::Digest Sha1::hash(std::span<const std::uint8_t> data) noexcept {
    Sha1 h;
    h.update(data);
    return h.final();
}

} // namespace yk