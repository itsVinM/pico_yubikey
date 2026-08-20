// Algo test harness for Renode — runs crypto algorithms on emulated RP2040,
// receives test vectors over UART, prints results as hex.
//
// Protocol (UART, 115200 baud, newline-delimited):
//   TX (device -> host): "READY\n"
//   RX (host -> device): "SHA1:<hex_msg>\n"
//                        "HMAC:<hex_key>,<hex_msg>\n"
//                        "HOTP:<hex_key>,<counter>,<digits>\n"
//                        "TOTP:<hex_key>,<time>,<period>,<digits>\n"
//   TX: "<cmd>:<hex_result>\n" or "<cmd>:FAIL\n"

#include "arch/rp2040/uart.hpp"
#include "core/hmac.hpp"
#include "core/otp.hpp"
#include "core/sha1.hpp"

#include <array>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <span>

static constexpr std::size_t kMaxLine = 512;

// Decode hex string to byte buffer. Returns number of bytes decoded.
static std::size_t hex_decode(std::span<char> hex, std::span<std::uint8_t> out) noexcept {
    std::size_t n = 0;
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        std::uint8_t hi = 0, lo = 0;
        auto [p1, e1] = std::from_chars(&hex[i], &hex[i + 1], hi, 16);
        auto [p2, e2] = std::from_chars(&hex[i + 1], &hex[i + 2], lo, 16);
        if (e1 != std::errc{} || e2 != std::errc{}) return n;
        out[n++] = static_cast<std::uint8_t>((hi << 4) | lo);
    }
    return n;
}

// Find ',' separator, return split point. Returns npos if not found.
static std::size_t find_char(std::span<char> s, char delim) noexcept {
    for (std::size_t i = 0; i < s.size(); i++)
        if (s[i] == delim) return i;
    return static_cast<std::size_t>(-1);
}

static void handle_sha1(yk::rp2040::Uart& uart, std::span<char> args) noexcept {
    std::array<std::uint8_t, 256> msg{};
    const std::size_t msg_len = hex_decode(args, msg);
    const auto digest = yk::core::Sha1::hash(std::span(msg.data(), msg_len));
    uart.puts("SHA1:");
    uart.put_hex_buf(digest);
    uart.puts("\n");
}

static void handle_hmac(yk::rp2040::Uart& uart, std::span<char> args) noexcept {
    const auto comma = find_char(args, ',');
    if (comma == static_cast<std::size_t>(-1)) { uart.puts("HMAC:FAIL\n"); return; }
    std::array<std::uint8_t, 128> key{};
    std::array<std::uint8_t, 256> msg{};
    const std::size_t key_len = hex_decode(args.first(comma), key);
    const std::size_t msg_len = hex_decode(args.subspan(comma + 1), msg);
    const auto mac = yk::core::hmac_sha1(
        std::span(key.data(), key_len), std::span(msg.data(), msg_len));
    uart.puts("HMAC:");
    uart.put_hex_buf(mac);
    uart.puts("\n");
}

static void handle_hotp(yk::rp2040::Uart& uart, std::span<char> args) noexcept {
    const auto c1 = find_char(args, ',');
    const auto c2 = find_char(args.subspan(c1 + 1), ',') + c1 + 1;
    if (c1 == static_cast<std::size_t>(-1) || c2 == static_cast<std::size_t>(-1)) {
        uart.puts("HOTP:FAIL\n"); return;
    }
    std::array<std::uint8_t, 128> key{};
    const std::size_t key_len = hex_decode(args.first(c1), key);
    std::uint64_t counter = 0;
    std::from_chars(args.data() + c1 + 1, args.data() + c2, counter);
    std::uint32_t digits = 6;
    std::from_chars(args.data() + c2 + 1, args.data() + args.size(), digits);
    const auto code = yk::core::otp::hotp(
        std::span(key.data(), key_len), counter, static_cast<std::uint8_t>(digits));
    uart.puts("HOTP:");
    uart.put_u32(code);
    uart.puts("\n");
}

static void handle_totp(yk::rp2040::Uart& uart, std::span<char> args) noexcept {
    const auto c1 = find_char(args, ',');
    const auto c2 = find_char(args.subspan(c1 + 1), ',') + c1 + 1;
    if (c1 == static_cast<std::size_t>(-1) || c2 == static_cast<std::size_t>(-1)) {
        uart.puts("TOTP:FAIL\n"); return;
    }
    std::array<std::uint8_t, 128> key{};
    const std::size_t key_len = hex_decode(args.first(c1), key);
    std::uint64_t time = 0;
    std::from_chars(args.data() + c1 + 1, args.data() + c2, time);
    std::uint64_t period = 30;
    std::from_chars(args.data() + c2 + 1, args.data() + args.size(), period);
    const auto code = yk::core::otp::totp(
        std::span(key.data(), key_len), time, period);
    uart.puts("TOTP:");
    uart.put_u32(code);
    uart.puts("\n");
}

int main() {
    yk::rp2040::Uart uart(115200);
    uart.puts("READY\n");

    std::array<char, kMaxLine> line{};

    for (;;) {
        auto span = uart.readln(line);
        if (span.empty()) continue;

        if (span.size() >= 5 && std::string_view(span.data(), 5) == "SHA1:") {
            handle_sha1(uart, span.subspan(5));
        } else if (span.size() >= 5 && std::string_view(span.data(), 5) == "HMAC:") {
            handle_hmac(uart, span.subspan(5));
        } else if (span.size() >= 5 && std::string_view(span.data(), 5) == "HOTP:") {
            handle_hotp(uart, span.subspan(5));
        } else if (span.size() >= 5 && std::string_view(span.data(), 5) == "TOTP:") {
            handle_totp(uart, span.subspan(5));
        } else {
            uart.puts("UNKNOWN\n");
        }
    }
}
