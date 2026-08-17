#pragma once

#include <cstdint>

namespace yk::rp2040 {

    // Minimal volatile register access wrapper. `volatile` stops the compiler
    // caching/reordering MMIO accesses. set/clear_bits is a read-modify-write
    // (NOT atomic) — use the RP2040 SET/CLR aliases where a race matters.
    class Reg {

    public:
        explicit constexpr Reg(std::uintptr_t address) noexcept : address_(address) {}
        [[nodiscard]] std::uint32_t read() const noexcept {
            return *reinterpret_cast<volatile std::uint32_t*>(address_);
        }
        void write(std::uint32_t value) const noexcept {
            *reinterpret_cast<volatile std::uint32_t*>(address_) = value;
        }
        void set_bits(std::uint32_t mask) const noexcept { write(read() | mask); }
        void clear_bits(std::uint32_t mask) const noexcept { write(read() & ~mask); }
    private:
        std::uintptr_t address_;
    };

    // DWT cycle counter (ARMv6-M, on RP2040). Counts CPU cycles while CYCCNTENA
    // is set — for microbenchmarks. Registers per ARM DDI0403.
    class CycleCounter {
        
    public:
        static void enable() noexcept {
            kDemcr.set_bits(kTrcena);     // gate DWT/ITM clocks; must be set first
            kCyccnt.write(0);             // clear stale count
            kCtrl.set_bits(kCyccntena);   // CYCCNTENA: start counting
        }
        [[nodiscard]] static std::uint32_t now() noexcept { return kCyccnt.read(); }

    private:
        inline static constexpr std::uint32_t kTrcena    = 0x01000000u;  // DEMCR bit 24
        inline static constexpr std::uint32_t kCyccntena = 1u;           // DWT_CTRL bit 0
        inline static constexpr Reg kDemcr{0xE000EDFCu};
        inline static constexpr Reg kCtrl{0xE0001000u};
        inline static constexpr Reg kCyccnt{0xE0001004u};
    };

} // namespace yk::rp2040