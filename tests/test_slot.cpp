#include "test.hpp"

#include "core/input.hpp"
#include "core/slot.hpp"

#include <cstdint>
#include <cstdlib>

TEST(press_scanner_short_press) {
    yk::core::PressScanner scan;
    CHECK_EQUAL(scan.update(true, 1000), yk::core::PressScanner::Event::none);
    CHECK_EQUAL(scan.update(true, 10'000), yk::core::PressScanner::Event::none);
    CHECK_EQUAL(scan.update(false, 150'000), yk::core::PressScanner::Event::none);
    CHECK_EQUAL(scan.update(false, 160'000), yk::core::PressScanner::Event::short_press);
}

TEST(press_scanner_long_press) {
    yk::core::PressScanner scan;
    CHECK_EQUAL(scan.update(true, 0), yk::core::PressScanner::Event::none);
    CHECK_EQUAL(scan.update(true, 50'000), yk::core::PressScanner::Event::none);
    CHECK_EQUAL(scan.update(true, 1'100'000), yk::core::PressScanner::Event::long_press);
    CHECK_EQUAL(scan.update(true, 1'700'000), yk::core::PressScanner::Event::repeat);
    CHECK_EQUAL(scan.update(false, 1'800'000), yk::core::PressScanner::Event::none);
    CHECK_EQUAL(scan.update(false, 1'810'000), yk::core::PressScanner::Event::none);
}

TEST(press_scanner_short_swallowed_after_long) {
    yk::core::PressScanner scan;
    CHECK_EQUAL(scan.update(true, 0), yk::core::PressScanner::Event::none);
    CHECK_EQUAL(scan.update(true, 50'000), yk::core::PressScanner::Event::none);
    CHECK_EQUAL(scan.update(true, 1'100'000), yk::core::PressScanner::Event::long_press);
    CHECK_EQUAL(scan.update(false, 1'300'000), yk::core::PressScanner::Event::none);
    CHECK_EQUAL(scan.update(false, 1'310'000), yk::core::PressScanner::Event::none);
}

TEST(press_scanner_bounce_ignored) {
    yk::core::PressScanner scan;
    CHECK_EQUAL(scan.update(true, 0), yk::core::PressScanner::Event::none);
    CHECK_EQUAL(scan.update(false, 1'000), yk::core::PressScanner::Event::none);
    CHECK_EQUAL(scan.update(true, 2'000), yk::core::PressScanner::Event::none);
    CHECK_EQUAL(scan.update(true, 50'000), yk::core::PressScanner::Event::none);
    CHECK_EQUAL(scan.update(false, 100'000), yk::core::PressScanner::Event::none);
    CHECK_EQUAL(scan.update(false, 110'000), yk::core::PressScanner::Event::short_press);
}

TEST(slot_execute_hotp_advances_counter) {
    yk::core::SlotConfig cfg;
    cfg.mode = yk::core::SlotMode::hotp;
    cfg.digits = 6;
    cfg.secret_len = 20;
    for (std::uint32_t i = 0; i < 20; i++)
        cfg.secret[i] = static_cast<std::uint8_t>("12345678901234567890"[i]);

    yk::core::SlotState state;
    const auto first = yk::core::execute_slot(cfg, state, 0);
    CHECK_EQUAL(first.len, 6u);
    CHECK_EQUAL(state.hotp_counter, 1u);
    const auto second = yk::core::execute_slot(cfg, state, 0);
    CHECK_EQUAL(state.hotp_counter, 2u);
    bool any_diff = false;
    for (std::uint32_t i = 0; i < 6; i++)
        if (first.text[i] != second.text[i]) any_diff = true;
    CHECK(any_diff);
}

TEST(slot_execute_totp_matches_direct) {
    yk::core::SlotConfig cfg;
    cfg.mode = yk::core::SlotMode::totp;
    cfg.digits = 8;
    cfg.period_secs = 30;
    cfg.secret_len = 20;
    for (std::uint32_t i = 0; i < 20; i++)
        cfg.secret[i] = static_cast<std::uint8_t>("12345678901234567890"[i]);

    yk::core::SlotState state;
    const auto out = yk::core::execute_slot(cfg, state, 1'700'000'123);
    const auto expected = yk::core::otp::totp(cfg.secret, 1'700'000'123, 30, 8);
    char digits[9] = {0};
    for (std::uint32_t i = 0; i < 8; i++)
        digits[i] = out.text[i];
    const auto got = static_cast<std::uint32_t>(std::strtoul(digits, nullptr, 10));
    CHECK_EQUAL(got, expected);
}

TEST(slot_execute_static_password) {
    yk::core::SlotConfig cfg;
    cfg.mode = yk::core::SlotMode::static_password;
    cfg.static_len = 5;
    for (std::uint32_t i = 0; i < 5; i++) cfg.static_text[i] = "hello"[i];
    yk::core::SlotState state;
    const auto out = yk::core::execute_slot(cfg, state, 0);
    CHECK_EQUAL(out.len, 5u);
    for (std::uint32_t i = 0; i < 5; i++) CHECK_EQUAL(out.text[i], "hello"[i]);
}

TEST(slot_execute_unset_is_empty) {
    yk::core::SlotConfig cfg;
    yk::core::SlotState state;
    const auto out = yk::core::execute_slot(cfg, state, 0);
    CHECK_EQUAL(out.len, 0u);
}
