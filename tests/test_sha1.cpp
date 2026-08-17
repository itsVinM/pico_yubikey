#include "test.hpp"

#include "core/hmac.hpp"
#include "core/sha1.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

using namespace yk::core;

namespace {
    std::string hex(const Sha1::Digest& digest){
        static const char* hex_chars = "0123456789abcdef";
        std::string result;
        for (const auto& byte : digest) {
            result += hex_chars[(byte >> 4) & 0x0F];
            result += hex_chars[byte & 0x0F];   
        }
        return result;
    }

    std::vector<std::uint8_t> bytes(std::string_view str) {
        return std::vector<std::uint8_t>(str.begin(), str.end());
    }

    std::vector<std::uint8_t> bytes_of(uint8_t value, size_t count) {
        return std::vector<std::uint8_t>(count, value);
    }
} // namespace


TEST(sha1_empty){
    CHECK_EQUAL(hex(Sha1::hash({})), "da39a3ee5e6b4b0d3255bfef95601890afd80709");
}

TEST(sha1_abc){
    const auto digest = Sha1::hash(bytes("abc"));
    CHECK_EQUAL(hex(digest), "a9993e364706816aba3e25717850c26c9cd0d89d");
}

TEST(sha1_two_block){
    const auto digest = Sha1::hash(bytes("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"));
    CHECK_EQUAL(hex(digest), "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
}

TEST(sha1_streaming_equals_oneshot) {
    const auto data = bytes("The quick brown fox jumps over the lazy dog");
    Sha1 sha1_str;
    sha1_str.update(std::span<const std::uint8_t>(data).first(10));
    sha1_str.update(std::span<const std::uint8_t>(data).subspan(10));
    CHECK_EQUAL(hex(sha1_str.final()),
               "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12");
}

TEST(hmac_sha1_rfc2202_case1) {
    // key = 0x0b * 20, msg = "Hi There"
    const auto key = bytes_of(0x0b, 20);
    const auto msg = bytes("Hi There");
    CHECK_EQUAL(hex(hmac_sha1(key, msg)),
               "b617318655057264e28bc0b6fb378c8ef146be00");
}

TEST(hmac_sha1_rfc2202_case2) {
    // key = "Jefe", msg = "what do ya want for nothing?"
    const auto key = bytes("Jefe");
    const auto msg = bytes("what do ya want for nothing?");
    CHECK_EQUAL(hex(hmac_sha1(key, msg)),
             "effcdf6ae5eb2fa2d27416d5f184df9c259a7c79");
}

TEST(hmac_sha1_key_longer_than_block) {
    // RFC 2202 case 7: key = 0xaa * 80 (pre-hashed), msg = "Test Using Larger Than Block-Size Key - Hash Key First"
    const auto key = bytes_of(0xaa, 80);
    const auto msg = bytes("Test Using Larger Than Block-Size Key - Hash Key First");
    CHECK_EQUAL(hex(hmac_sha1(key, msg)),
             "aa4ae5e15272d00e95705637ce8a3b55ed402112");
}
