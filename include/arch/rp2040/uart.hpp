#pragma once

#include "pico/stdlib.h"
#include "hardware/uart.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace yk::rp2040 {

    // Minimal UART driver for RP2040 (UART0, GP0 TX / GP1 RX by default).
    class Uart {
    public:
        explicit Uart(std::uint32_t baud = 115200) noexcept {
            uart_init(uart0, baud);
            gpio_set_function(TX_PIN, GPIO_FUNC_UART);
            gpio_set_function(RX_PIN, GPIO_FUNC_UART);
        }

        void putc(char c) noexcept {
            uart_putc_raw(uart0, static_cast<std::uint8_t>(c));
        }

        void puts(std::string_view sv) noexcept {
            for (char c : sv) putc(c);
        }

        void put_hex8(std::uint8_t v) noexcept {
            static constexpr char kHex[] = "0123456789abcdef";
            putc(kHex[(v >> 4) & 0x0F]);
            putc(kHex[v & 0x0F]);
        }

        void put_hex_buf(std::span<const std::uint8_t> data) noexcept {
            for (auto b : data) put_hex8(b);
        }

        void put_u32(std::uint32_t v) noexcept {
            char buf[11];
            int i = 10;
            buf[i] = '\0';
            if (v == 0) { buf[--i] = '0'; }
            else { while (v > 0) { buf[--i] = '0' + (v % 10); v /= 10; } }
            puts(&buf[i]);
        }

        [[nodiscard]] bool available() const noexcept {
            return uart_is_readable(uart0);
        }

        [[nodiscard]] char getc() noexcept {
            return static_cast<char>(uart_getc(uart0));
        }

        // Read a line (up to '\n' or buf.size()-1 chars). Returns span of chars read.
        std::span<char> readln(std::span<char> buf) noexcept {
            std::size_t n = 0;
            while (n + 1 < buf.size()) {
                while (!uart_is_readable(uart0)) {}
                char c = static_cast<char>(uart_getc(uart0));
                if (c == '\n' || c == '\r') break;
                buf[n++] = c;
            }
            return buf.first(n);
        }

    private:
        static inline constexpr std::uint32_t TX_PIN = 0;
        static inline constexpr std::uint32_t RX_PIN = 1;
    };

} // namespace yk::rp2040
