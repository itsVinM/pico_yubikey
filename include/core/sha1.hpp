#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace yk::core {
    class Sha1 {
    public:
        using Digest = std::array<std::uint8_t, 20>;
        static constexpr std::size_t kBlockSize = 64;

        Sha1() noexcept{reset();};

        void reset() noexcept;
        void update(std::span<const std::uint8_t> data) noexcept;
        [[nodiscard]] Digest final() noexcept;
        [[nodiscard]] static Digest hash(std::span<const std::uint8_t> data) noexcept;
    private:
        std::array<std::uint32_t, 5> m_state{};
        std::array<std::uint8_t, kBlockSize> m_buffer{};
        void compress(const std::array<std::uint8_t, kBlockSize>& block) noexcept;
        std::size_t buffer_len_ = 0;
        std::uint64_t total_len_ = 0;
    };
} // namespace yk::core