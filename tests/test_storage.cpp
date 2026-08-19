#include "test.hpp"

#include "core/storage.hpp"

#include <array>
#include <cstdint>

namespace {

std::array<std::uint8_t, yk::core::storage::kRecordBytes> buffer{};

yk::core::storage::Record make_record() {
    yk::core::storage::Record r{
        .config = {
            .mode = yk::core::SlotMode::hotp,
            .digits = 8,
            .period_secs = 30,
            .secret_len = 20,
        },
        .state = {.hotp_counter = 4242},
    };
    for (std::uint32_t i = 0; i < 20; i++)
        r.config.secret[i] = static_cast<std::uint8_t>(i * 7 + 1);
    return r;
}

} // namespace

TEST(storage_roundtrip) {
    const auto rec = make_record();
    yk::core::storage::serialize(buffer, rec, 0);

    yk::core::storage::Record out{};
    CHECK(yk::core::storage::deserialize(buffer, out, 0));
    CHECK_EQUAL(static_cast<int>(out.config.mode), static_cast<int>(yk::core::SlotMode::hotp));
    CHECK_EQUAL(out.config.digits, 8);
    CHECK_EQUAL(out.config.period_secs, 30u);
    CHECK_EQUAL(out.config.secret_len, 20u);
    CHECK_EQUAL(out.state.hotp_counter, 4242u);
    for (std::uint32_t i = 0; i < 20; i++)
        CHECK_EQUAL(out.config.secret[i], static_cast<std::uint8_t>(i * 7 + 1));
}

TEST(storage_absent_record) {
    buffer.fill(0xFF);
    yk::core::storage::Record out{};
    CHECK(yk::core::storage::is_absent(buffer));
    CHECK(!yk::core::storage::deserialize(buffer, out, 0));
}

TEST(storage_corruption_detected) {
    const auto rec = make_record();
    yk::core::storage::serialize(buffer, rec, 0);
    buffer[5] ^= 0x55;
    yk::core::storage::Record out{};
    CHECK(!yk::core::storage::deserialize(buffer, out, 0));
}

TEST(storage_slot_index_mismatch) {
    const auto rec = make_record();
    yk::core::storage::serialize(buffer, rec, 1);
    yk::core::storage::Record out{};
    CHECK(!yk::core::storage::deserialize(buffer, out, 0));
    CHECK(yk::core::storage::deserialize(buffer, out, 1));
}

TEST(storage_static_password_roundtrip) {
    yk::core::storage::Record r{
        .config = {
            .mode = yk::core::SlotMode::static_password,
            .static_len = 12,
        },
        .state = {},
    };
    const char* text = "correct horse";
    for (std::uint32_t i = 0; i < 12; i++)
        r.config.static_text[i] = text[i];
    yk::core::storage::serialize(buffer, r, 1);

    yk::core::storage::Record out{};
    CHECK(yk::core::storage::deserialize(buffer, out, 1));
    CHECK_EQUAL(out.config.static_len, 12u);
    for (std::uint32_t i = 0; i < 12; i++)
        CHECK_EQUAL(out.config.static_text[i], text[i]);
}
