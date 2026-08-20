#include "arch/rp2040/gpio.hpp"
#include "arch/rp2040/uart.hpp"
#include "arch/rp2040/flash.hpp"
#include "core/command.hpp"
#include "core/input.hpp"

#include "pico/stdlib.h"
#include <array>
#include <cstdint>
#include <cstring>

int main() {
    stdio_init_all();

    yk::rp2040::Uart uart(115200);
    yk::rp2040::Led led(25);
    yk::rp2040::Button button(15);

    yk::core::cmd::Session session{};
    yk::rp2040::FlashStore store;
    store.load(session);

    yk::core::PressScanner scanner;

    uart.puts("pico-yubikey\n");
    led.on();

    std::array<std::uint8_t, 256> in_buf{};
    std::array<std::uint8_t, 256> out_buf{};
    std::size_t in_len = 0;
    bool led_state = false;

    for (;;) {
        // Button scan
        const bool pressed = button.is_pressed();
        const std::uint64_t now_us = to_us_since_boot(get_absolute_time());
        const auto event = scanner.update(pressed, now_us);

        if (event == yk::core::PressScanner::Event::short_press ||
            event == yk::core::PressScanner::Event::long_press) {
            const std::uint64_t epoch = yk::core::cmd::epoch_secs(session, now_us / 1'000'000);
            const auto text = yk::core::execute_slot(session.slots[0], session.states[0], epoch);
            // TODO: send text via USB HID keyboard
            led_state = !led_state;
            led_state ? led.on() : led.off();
        }

        // UART config protocol — same binary frames as USB CDC
        while (uart.available()) {
            char c = uart.getc();
            if (c == yk::core::cmd::kFrameEnd) {
                if (in_len > 0) {
                    const std::uint64_t epoch =
                        yk::core::cmd::epoch_secs(session, now_us / 1'000'000);
                    const auto n = yk::core::cmd::dispatch(
                        session,
                        std::span<const std::uint8_t>(in_buf.data(), in_len),
                        out_buf, epoch);
                    if (n > 0) {
                        uart.puts(std::string_view(
                            reinterpret_cast<const char*>(out_buf.data()), n));
                    }
                    in_len = 0;
                }
            } else if (in_len < in_buf.size()) {
                in_buf[in_len++] = static_cast<std::uint8_t>(c);
            }
        }

        // Save to flash when config changes
        if (session.config_dirty) {
            store.save(session);
            session.config_dirty = false;
        }

        sleep_ms(1);
    }
}
