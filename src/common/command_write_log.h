// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <utility>
#include <vector>

#include "common/common_types.h"

namespace Common {

/// GPU writes to memory that holds submitted NX2 command words (DmaPusher runs copies of them),
/// by host address: the same memory can be mapped at several GPU and device addresses. CPU writes
/// are not logged, so a copy the CPU refilled since stays usable and one the GPU rewrote doesn't.
class CommandWriteLog {
public:
    /// The pages a command list marked, unmarked when it is destroyed (once it has run): memory
    /// reused for other data afterwards does not fill the log.
    class Marks {
    public:
        Marks() = default;
        Marks(Marks&& other) noexcept : ranges{std::exchange(other.ranges, {})} {}
        Marks& operator=(Marks&& other) noexcept {
            if (this != &other) {
                Release();
                ranges = std::exchange(other.ranges, {});
            }
            return *this;
        }
        Marks(const Marks&) = delete;
        Marks& operator=(const Marks&) = delete;
        ~Marks() {
            Release();
        }

        void Add(const u8* pointer, std::size_t size);

    private:
        void Release();

        std::vector<std::pair<const u8*, std::size_t>> ranges;
    };

    static CommandWriteLog& Get();

    /// Logs the write if it touches a marked page (hashed: rarely others too).
    void Note(const u8* pointer, std::size_t size) {
        if (size == 0) {
            return;
        }
        const u64 first = Page(pointer);
        const u64 last = Page(pointer + size - 1);
        for (u64 page = first; page <= last; ++page) {
            if (marks[FilterIndex(page)].load(std::memory_order_relaxed) != 0) {
                Log(pointer, size);
                return;
            }
        }
    }

    /// The last logged write, to compare against later.
    [[nodiscard]] u64 Sequence() const noexcept {
        return sequence.load(std::memory_order_acquire);
    }

    enum class Result {
        Unwritten,
        Written,
        Unknown, ///< The log no longer reaches back to `since`
    };

    /// Whether a write after `since` touched [pointer, pointer + size).
    [[nodiscard]] Result WrittenSince(const u8* pointer, std::size_t size, u64 since) const;

private:
    static constexpr u64 page_bits = 12;
    static constexpr std::size_t filter_size = 1 << 14;
    static constexpr std::size_t log_size = 16384;

    struct Write {
        std::uintptr_t begin;
        std::uintptr_t end;
        u64 sequence;
    };

    [[nodiscard]] static u64 Page(const u8* pointer) noexcept {
        return reinterpret_cast<std::uintptr_t>(pointer) >> page_bits;
    }
    [[nodiscard]] static std::size_t FilterIndex(u64 page) noexcept {
        return page & (filter_size - 1);
    }

    void Log(const u8* pointer, std::size_t size);
    void Mark(const u8* pointer, std::size_t size, bool add);

    /// Per filter slot, how many submitted pages not yet run use it.
    std::array<std::atomic<u32>, filter_size> marks{};
    /// Per filter slot, the sequence of the last logged write to it (0: none).
    std::array<std::atomic<u64>, filter_size> slot_writes{};
    std::array<Write, log_size> writes{};
    std::atomic<u64> sequence{};
    mutable std::mutex mutex;
};

} // namespace Common
