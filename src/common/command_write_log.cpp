// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/command_write_log.h"

namespace Common {

CommandWriteLog& CommandWriteLog::Get() {
    static CommandWriteLog instance;
    return instance;
}

void CommandWriteLog::Mark(const u8* pointer, std::size_t size) {
    if (size == 0) {
        return;
    }
    const u64 first = Page(pointer);
    const u64 last = Page(pointer + size - 1);
    for (u64 page = first; page <= last; ++page) {
        marked[FilterIndex(page)].store(1, std::memory_order_relaxed);
    }
}

void CommandWriteLog::Log(const u8* pointer, std::size_t size) {
    std::scoped_lock lk{mutex};
    const u64 next = sequence.load(std::memory_order_relaxed) + 1;
    const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
    writes[next % log_size] = {begin, begin + size, next};
    const u64 first = Page(pointer);
    const u64 last = Page(pointer + size - 1);
    for (u64 page = first; page <= last; ++page) {
        slot_writes[FilterIndex(page)].store(next, std::memory_order_relaxed);
    }
    // Published after the write itself: a copy taken at this sequence or later holds it.
    sequence.store(next, std::memory_order_release);
}

CommandWriteLog::Result CommandWriteLog::WrittenSince(const u8* pointer, std::size_t size,
                                                     u64 since) const {
    if (size == 0 || sequence.load(std::memory_order_acquire) == since) {
        return Result::Unwritten;
    }
    const u64 first = Page(pointer);
    const u64 last = Page(pointer + size - 1);
    bool touched = false;
    for (u64 page = first; page <= last && !touched; ++page) {
        touched = slot_writes[FilterIndex(page)].load(std::memory_order_relaxed) > since;
    }
    if (!touched) {
        return Result::Unwritten;
    }
    std::scoped_lock lk{mutex};
    const u64 current = sequence.load(std::memory_order_relaxed);
    if (current - since > log_size) {
        return Result::Unknown;
    }
    const auto begin = reinterpret_cast<std::uintptr_t>(pointer);
    const auto end = begin + size;
    for (u64 index = since + 1; index <= current; ++index) {
        const Write& write = writes[index % log_size];
        if (write.begin < end && begin < write.end) {
            return Result::Written;
        }
    }
    return Result::Unwritten;
}

} // namespace Common
