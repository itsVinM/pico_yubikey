#include "test.hpp"

#include "core/otp.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace {

std::vector<std::uint8_t> secret() {
    const std::string s = "12345678901234567890";
    return {s.begin(), s.end()};
}

} // namespace

TEST(hotp_rfc4226_vectors) {
    // RFC 4226 Appendix D: 6-digit HOTP, secret = ASCII "12345678901234567890".
    const std::uint32_t expected[10] = {755224, 287082, 359152, 969429,
                                        338314, 254676, 287922, 162583,
                                        399871, 520489};
    const auto key = secret();
    for (std::uint64_t c = 0; c < 10; c++)
        CHECK_EQUAL(yk::core::otp::hotp(key, c, 6), expected[c]);
}

TEST(hotp_digit_widths) {
    const auto key = secret();
    const std::uint32_t six = yk::core::otp::hotp(key, 0, 6);
    const std::uint32_t seven = yk::core::otp::hotp(key, 0, 7);
    const std::uint32_t eight = yk::core::otp::hotp(key, 0, 8);
    CHECK_EQUAL(six, 755224);
    CHECK_EQUAL(seven, 4755224);
    CHECK_EQUAL(eight, 84755224);
    // Truncated widths must be consistent (RFC 4226 truncate is width-agnostic).
    CHECK_EQUAL(six % 1000000, seven % 1000000);
    CHECK_EQUAL(seven % 1000000, eight % 1000000);
}

TEST(totp_rfc6238_sha1_vectors) {
    // RFC 6238 Appendix B: SHA-1, 8 digits, period 30.
    const auto key = secret();
    struct V {
        std::uint64_t time;
        std::uint32_t code;
    };
    const V cases[] = {{59, 94287082},      {1111111109, 7081804},
                       {1111111111, 14050471}, {1234567890, 89005924},
                       {2000000000, 69279037}, {20000000000, 65353130}};
    for (const auto& c : cases)
        CHECK_EQUAL(yk::core::otp::totp(key, c.time, 30, 8), c.code);
}

TEST(hotp_counter_block_endian) {
    // Counter 0 -> 8 zero bytes; counter 1 -> 0x0000000000000001.
    const auto c0 = yk::core::otp::counter_block(0);
    const auto c1 = yk::core::otp::counter_block(1);
    CHECK_EQUAL(c1[7], 1);
    for (const auto b : c0) CHECK_EQUAL(b, 0);
    CHECK_EQUAL(c1[0], 0);
    CHECK_EQUAL(c1[6], 0);
}

TEST(totp_period_large) {
    // A 100s period changes which counter is used.
    const auto key = secret();
    const std::uint32_t a = yk::core::otp::totp(key, 100, 100, 8);
    const std::uint32_t b = yk::core::otp::totp(key, 199, 100, 8);
    CHECK_EQUAL(a, b);
}
