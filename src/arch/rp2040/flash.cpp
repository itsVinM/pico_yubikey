#include "arch/rp2040/flash.hpp"
#include "hardware/flash.h"
#include <cstring>

namespace yk::rp2040 {

namespace storage = yk::core::storage;
using yk::core::cmd::Session;

void FlashStore::load(Session& session) const noexcept {
    load_one(session, 0);
    load_one(session, 1);
}

bool FlashStore::save(const Session& session) const noexcept {
    alignas(4) std::uint8_t page[kPageBytes]{};

    storage::Record r0{
        .config = session.slots[0],
        .state = session.states[0],
    };
    storage::Record r1{
        .config = session.slots[1],
        .state = session.states[1],
    };
    storage::serialize(std::span<std::uint8_t>(page, kPageBytes), r0, 0);
    storage::serialize(std::span<std::uint8_t>(page + kRecordSlotBytes, kRecordSlotBytes), r1, 1);

    const std::uintptr_t base = kFlashDataXip;
    flash_range_erase(base - XIP_BASE, kPageBytes);
    flash_range_program(base - XIP_BASE, page, kPageBytes);
    return true;
}

void FlashStore::load_one(Session& session, std::uint32_t slot_index) const noexcept {
    auto* base = reinterpret_cast<const std::uint8_t*>(kFlashDataXip + slot_index * kRecordSlotBytes);
    std::span<const std::uint8_t> page(base, kRecordSlotBytes);

    if (storage::is_absent(page)) return;

    storage::Record rec{};
    if (storage::deserialize(page, rec, slot_index)) {
        session.slots[slot_index] = rec.config;
        session.states[slot_index] = rec.state;
    }
}

} // namespace yk::rp2040