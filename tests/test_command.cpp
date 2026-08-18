#include "test.hpp"

#include "core/command.hpp"
#include "core/hmac.hpp"
#include "core/otp.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace {

const std::uint8_t kSecret20[20] = {
    0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
    0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30};

std::vector<std::uint8_t> frame(std::uint8_t cmd) { return {cmd}; }

} // namespace

TEST(cmd_get_status_empty) {
    yk::core::cmd::Session s;
    std::array<std::uint8_t, 16> resp{};
    auto req = frame(static_cast<std::uint8_t>(yk::core::cmd::CmdId::get_status));
    const auto n = yk::core::cmd::dispatch(s, req, resp, 1000);
    CHECK_EQUAL(n, 4u);
    CHECK_EQUAL(resp[0], static_cast<std::uint8_t>(yk::core::cmd::Status::ok));
    CHECK_EQUAL(resp[1], 2u);
    CHECK_EQUAL(resp[2], static_cast<std::uint8_t>(yk::core::SlotMode::unset));
    CHECK_EQUAL(resp[3], static_cast<std::uint8_t>(yk::core::SlotMode::unset));
}

TEST(cmd_set_and_get_slot) {
    yk::core::cmd::Session s;
    std::array<std::uint8_t, 64> resp{};

    std::vector<std::uint8_t> req =
        frame(static_cast<std::uint8_t>(yk::core::cmd::CmdId::set_slot));
    req.push_back(0);
    req.push_back(static_cast<std::uint8_t>(yk::core::SlotMode::totp));
    req.push_back(8);
    req.insert(req.end(), {30, 0, 0, 0});
    req.push_back(20);
    req.insert(req.end(), kSecret20, kSecret20 + 20);
    CHECK_EQUAL(yk::core::cmd::dispatch(s, req, resp, 0), 1u);
    CHECK_EQUAL(resp[0], static_cast<std::uint8_t>(yk::core::cmd::Status::ok));

    auto get = frame(static_cast<std::uint8_t>(yk::core::cmd::CmdId::get_slot));
    get.push_back(0);
    const auto n = yk::core::cmd::dispatch(s, get, resp, 0);
    CHECK_EQUAL(n, 27u);
    CHECK_EQUAL(resp[0], static_cast<std::uint8_t>(yk::core::cmd::Status::ok));
    CHECK_EQUAL(resp[1], static_cast<std::uint8_t>(yk::core::SlotMode::totp));
    CHECK_EQUAL(resp[2], 8u);
    CHECK_EQUAL(resp[3], 30u);
    CHECK_EQUAL(resp[4], 0u);
    CHECK_EQUAL(resp[5], 0u);
    CHECK_EQUAL(resp[6], 0u);
    for (int i = 0; i < 20; i++) CHECK_EQUAL(resp[7 + i], kSecret20[i]);
}

TEST(cmd_clear_slot) {
    yk::core::cmd::Session s;
    std::array<std::uint8_t, 16> resp{};

    auto req = frame(static_cast<std::uint8_t>(yk::core::cmd::CmdId::clear_slot));
    req.push_back(1);
    CHECK_EQUAL(yk::core::cmd::dispatch(s, req, resp, 0), 1u);
    CHECK_EQUAL(resp[0], static_cast<std::uint8_t>(yk::core::cmd::Status::ok));
}

TEST(cmd_bad_slot_rejected) {
    yk::core::cmd::Session s;
    std::array<std::uint8_t, 16> resp{};
    auto req = frame(static_cast<std::uint8_t>(yk::core::cmd::CmdId::clear_slot));
    req.push_back(9);
    CHECK_EQUAL(yk::core::cmd::dispatch(s, req, resp, 0), 1u);
    CHECK_EQUAL(resp[0], static_cast<std::uint8_t>(yk::core::cmd::Status::bad_slot));
}

TEST(cmd_challenge_response) {
    yk::core::cmd::Session s;
    std::array<std::uint8_t, 64> resp{};

    auto set = frame(static_cast<std::uint8_t>(yk::core::cmd::CmdId::set_slot));
    set.push_back(1);
    set.push_back(static_cast<std::uint8_t>(yk::core::SlotMode::challenge));
    set.push_back(0);
    set.insert(set.end(), {0, 0, 0, 0});
    set.push_back(20);
    set.insert(set.end(), kSecret20, kSecret20 + 20);
    CHECK_EQUAL(yk::core::cmd::dispatch(s, set, resp, 0), 1u);

    const char* challenge = "arbitrary challenge bytes";
    auto req = frame(static_cast<std::uint8_t>(yk::core::cmd::CmdId::challenge));
    req.push_back(1);
    req.push_back(static_cast<std::uint8_t>(std::string(challenge).size()));
    req.insert(req.end(), challenge, challenge + std::string(challenge).size());
    const auto n = yk::core::cmd::dispatch(s, req, resp, 0);
    CHECK_EQUAL(n, 21u);
    CHECK_EQUAL(resp[0], static_cast<std::uint8_t>(yk::core::cmd::Status::ok));

    const auto expect = yk::core::hmac_sha1(
        std::span<const std::uint8_t>(kSecret20, 20),
        std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(challenge),
                                      std::string(challenge).size()));
    for (std::size_t i = 0; i < expect.size(); i++)
        CHECK_EQUAL(resp[1 + i], expect[i]);
}

TEST(cmd_time_offset) {
    yk::core::cmd::Session s;
    std::array<std::uint8_t, 16> resp{};

    const std::uint64_t now = yk::core::cmd::epoch_secs(s, 500);
    CHECK_EQUAL(now, 500u);

    std::vector<std::uint8_t> req =
        frame(static_cast<std::uint8_t>(yk::core::cmd::CmdId::set_time));
    const std::uint64_t target = 1'700'000'000;
    for (int i = 0; i < 8; i++) req.push_back(static_cast<std::uint8_t>(target >> (8 * i)));
    CHECK_EQUAL(yk::core::cmd::dispatch(s, req, resp, now), 1u);
    CHECK_EQUAL(resp[0], static_cast<std::uint8_t>(yk::core::cmd::Status::ok));

    CHECK_EQUAL(yk::core::cmd::epoch_secs(s, 600), target + 100);
}

TEST(cmd_unknown_command) {
    yk::core::cmd::Session s;
    std::array<std::uint8_t, 16> resp{};
    std::vector<std::uint8_t> req = frame(0x7F);
    CHECK_EQUAL(yk::core::cmd::dispatch(s, req, resp, 0), 1u);
    CHECK_EQUAL(resp[0], static_cast<std::uint8_t>(yk::core::cmd::Status::bad_command));
}
