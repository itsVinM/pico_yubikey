#pragma once

#include "core/sha1.hpp"

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>

namespace yk::core {
    inline Sha1::Digest hmac_sha1(std::span<const std::uint8_t> key, std::span<const std::uint8_t> message) noexcept {
        std::array<std::uint8_t, Sha1::kBlockSize> key_block{};
        if (key.size() > Sha1::kBlockSize) {
            const auto digest = Sha1::hash(key);
            key_block.fill(0);
            std::copy(digest.begin(), digest.end(), key_block.begin());
        } else {
            key_block.fill(0);
            std::copy(key.begin(), key.end(), key_block.begin());
        }

        std::array<std::uint8_t, Sha1::kBlockSize> ipad{};
        std::array<std::uint8_t, Sha1::kBlockSize> opad{};
        for (std::size_t i = 0; i < Sha1::kBlockSize; i++) {
            ipad[i] = static_cast<std::uint8_t>(key_block[i] ^ 0x36);
            opad[i] = static_cast<std::uint8_t>(key_block[i] ^ 0x5c);
        }

        Sha1 inner;
        inner.update(ipad);
        inner.update(message);
        const auto inner_digest = inner.final();

        Sha1 outer;
        outer.update(opad);
        outer.update(inner_digest);
        return outer.final();
    }
} // namespace yk::core