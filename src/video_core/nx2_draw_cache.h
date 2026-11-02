// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdlib>

namespace VideoCommon {

/// Per-draw caches and prechecks on the GPU (back) thread: skipped invalidations, reused buffer
/// and texture bindings. EDEN_NX2_DRAW_CACHE=0 turns them all off, to compare or rule them out.
inline const bool g_draw_cache = [] {
    const char* value = std::getenv("EDEN_NX2_DRAW_CACHE");
    return value == nullptr || value[0] != '0';
}();

} // namespace VideoCommon
