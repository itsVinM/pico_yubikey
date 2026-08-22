#include "arch/rp2040/gpio.hpp"
#include "arch/rp2040/uart.hpp"
#include "arch/rp2040/flash.hpp"
#include "arch/rp2040/usb.hpp"
#include "core/command.hpp"
#include "core/input.hpp"
#include "core/slot.hpp"

#include "pico/stdlib.h"
#include <array>
#include <cstdint>
#include <cstring>

namespace {

using yk::core::cmd::Session;
using yk::rp2040::FlashStore;

// Short press = slot 1, long press = slot 2 (YubiKey convention).
void press_slot(Session& session, FlashStore& flash, yk::core::Slot slot,
                std::uint64_t epoch_now_secs) noexcept {
    const std::uint32_t idx = static_cast<std::uint32_t>(slot);
    auto& cfg = session.slots[idx];
    auto& state = session.states[idx];
    if (cfg.mode == yk::core::SlotMode::unset) return;

    const auto text = yk::core::execute_slot(cfg, state, epoch_now_secs);
    if (text.len == 0) return;

    if (cfg.mode == yk::core::SlotMode::hotp) session.config_dirty = true;  // counter moved
    yk::rp2040::usb_type_text({text.text.data(), text.len});

    // Persist any state change (HOTP counter) once the loop turns.
    if (session.config_dirty) flash.save(session);
    session.config_dirty = false;
}

} // namespace

int main() {
    stdio_init_all();

    yk::rp2040::Uart uart(115200);
    yk::rp2040::Led led(25);
    yk::rp2040::Button button(15);

    Session session{};
    FlashStore store;
    store.load(session);

    yk::core::PressScanner scanner;

    uart.puts("pico-yubikey\n");
    led.on();

    yk::rp2040::usb_init();

    std::array<std::uint8_t, 256> in_buf{};
    std::array<std::uint8_t, 256> out_buf{};
    std::size_t in_len = 0;

    for (;;) {
        const bool pressed = button.is_pressed();
        const std::uint64_t now_us = to_us_since_boot(get_absolute_time());
        const std::uint64_t epoch = yk::core::cmd::epoch_secs(session, now_us / 1'000'000);

        yk::rp2040::usb_poll(session, epoch);

        switch (scanner.update(pressed, now_us)) {
            case yk::core::PressScanner::Event::short_press:
                press_slot(session, store, yk::core::Slot::slot1, epoch);
                break;
            case yk::core::PressScanner::Event::long_press:
                press_slot(session, store, yk::core::Slot::slot2, epoch);
                break;
            case yk::core::PressScanner::Event::repeat:
            case yk::core::PressScanner::Event::none:
                break;
        }

        // UART config protocol — same binary frames as USB CDC
        while (uart.available()) {
            char c = uart.getc();
            if (c == static_cast<char>(yk::core::cmd::kFrameEnd)) {
                if (in_len > 0) {
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
