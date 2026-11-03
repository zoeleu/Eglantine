# Main session status (coordinator)

Untracked notes file, do not commit. Last updated 2026-11-02.

## Done and pushed (zoeleu/Eglantine)
- PR #1 (hotfix/amd-sparse-lazy-crash) merged into nx2 as adfce8e923. Contains:
  - merge of nx2 into the branch (conflict resolution, bf936b0a8c)
  - 434c71da84 command snapshot write log + native shader capture flushes (Codex review 1)
  - 408340ea78 write log keyed by host address, device memory and span writes logged (Codex review 2)
  - 4f093d922f marks released after a command list runs, keep the copy when the log
    overflows, log size 16384 (fixes ~14,000 overflow fallbacks seen in a game run)
- PR comments posted for each round (conflict summary, two review replies).

## Benchmark data so far (title 0400C3F00006E000, ~5 min each, profiling on)
- 434c71da84: 37.7 fps after 60 s, p99 52.9 ms, 1% low 13.0 fps
- 408340ea78: 39.4 fps after 60 s, p99 49.8 ms, 1% low 13.3 fps (single runs: treat as a wash)
- CSVs in ~/.local/share/eden/log/. record_frame_times=true is set in
  ~/.config/eden/qt-config.ini (close all emulators before editing the config).
- Profiles were back-thread bound; front waited ~550 ms/s on semaphore-acquire drains.
  Old profile numbers are inflated by fenced clock reads (fixed in ca32ec64c5).

## In progress: back-thread performance (all unpushed)
Base branch perf/back-thread in this checkout (/home/zoe/Projects/Eglantine), PUSHED by the
user to origin/perf/back-thread:
- ca32ec64c5 profiler: unfenced TSC, one zone per back batch
- 9c82b9487e command write log: sequence and mutex on their own cache lines (user's commit)

Parallel agents, each in its own worktree branched from ca32ec64c5. Each keeps
AGENT_STATUS.md in its worktree root:
- /home/zoe/Projects/Eglantine-perf-ring (perf/ring): direct Maxwell3D calls, batched macro
  state writes, merged cb_data words, ring prefetch, FinishFrontMacro flag, inline
  DeferFunction storage, Report() only when waiting.
- /home/zoe/Projects/Eglantine-perf-draw (perf/draw): duplicate pipeline lookup, debug gate,
  cheaper FlushCaching/InnerInvalidation, SynchronizeBuffer skip, storage buffer and texture
  view caches, handle reads via host pointer, compute pipeline cache, upload counters,
  fixed storage for last_descriptor_payload.
- /home/zoe/Projects/Eglantine-perf-sync (perf/sync): guarded AcquireGequal fast path with
  in-flight release tracking (the unguarded "skip drain if memory matches" is UNSAFE, see
  below), non-blocking CanResumeAcquire, progress-aware RunThread, atomic Latched, waiter
  cache-line split, queued-release visibility, WaitFor(position) for macro DMA reads.
Risky changes are meant to sit behind EDEN_NX2_* env var kill switches (see each status file).

Agent state when the 5-hour usage limit hit (resets 1am America/Sao_Paulo):
- perf/sync: FINISHED. 4 commits (8ebee87db7 deferred acquire check on the back,
  6d0386caa4 no pending sleep while back runs, 838b27c93b lock-free waiter polling,
  eee02ac61d WaitForEffects before macro DMA reads). Kill switches
  EDEN_NX2_ACQUIRE_FASTPATH=0, EDEN_NX2_DMA_READ_WAITFOR=0. Builds, no new test failures.
  Skipped the raw-read GEQ fast path and the queued-release table (P3) as unsafe; details in
  its AGENT_STATUS.md.
- perf/draw: stopped by the limit; see its AGENT_STATUS.md (was about to do the
  last_descriptor_payload item).
- perf/ring: stopped by the limit with NO commits, only an uncommitted diff (5 files); the
  coordinator wrote its AGENT_STATUS.md from git. Review the diff before continuing.
- port/zortos-fixes: stopped by the limit while building; see its AGENT_STATUS.md.
To continue an agent after the reset, message it from the main session (SendMessage) so it
resumes with its context, or start fresh from its status file.

Next steps once agents finish:
1. Read each worktree's AGENT_STATUS.md and `git log --oneline ca32ec64c5..perf/<name>`.
2. Merge perf/ring, perf/draw, perf/sync into perf/back-thread (expect small conflicts in
   gpu_split.h/.cpp between ring and sync).
3. Build with ccache (`CCACHE_BASEDIR=/home/zoe/Projects ninja -C build -j12`), run
   build/bin/tests (baseline: 9 failing cases, memory_tracker x8 + core_timing).
4. Benchmark in game the same way as before (EDEN_NX2_GPU_PROFILE=1 and once without) and
   compare CSVs; test each kill switch if something breaks (hangs, flicker, stale textures).
5. Only then push and open a PR (ask the user first).

Key safety notes for the acquire work (from the adversarial review, verified):
- Skipping the drain when memory already matches is unsafe without guards: a reset of the
  semaphore may still be queued on the back ring; FlushRegion on the front touches
  back-owned caches; ReleaseFences(false) is a no-op on Vulkan; SEM_EXECUTE never suspends.
- Safe only for AcquireGequal with no in-flight write to that address and no GPU-modified
  data for it.

## In progress: zortos293 fork port
- /home/zoe/Projects/Eglantine-zortos, branch port/zortos-fixes (unpushed), status in its
  AGENT_STATUS.md. Remote `zortos293` added in this repo.
- e4b3a460f8 ported: unit test fixes (memory_tracker mock, core_timing reset, AVX-512 CLZ,
  device memory manager ASID guard incl. `asid = asid_2;`) - should fix our 9 failing tests.
- Agent was porting: 1154fb7d7a, 98d524f704, badc91ebce (pipeline/cache overhead),
  3140648a66 (Mario Tennis Fever).
- Deliberately skipped (user chose targeted scope): 60ef6d2750 (11.7k-line undocumented
  import), 11a110898f and 4285e58d06 (self-described unverified), 75883be7d0 ("unfinished"
  checkpoint), macOS CI/signing/HTTPS work, README/updater/AGENTS.md removal.
- Ported commit messages: keep original author and human co-authors, capy-ai Co-authored-by
  turned into "Assisted-by: Capy ...", plus "(cherry picked from zortos293/Eglantine commit X)"
  and "Assisted-by: Claude Opus 5.5 (porting the change)" (AI policy bans AI co-authors).

## Other worktrees
- /home/zoe/Projects/Eglantine-bench: detached at 434c71da84, built, used as benchmark
  baseline. Remove with `git worktree remove ../Eglantine-bench` when no longer needed.

## Rules for whoever continues
- AGENTS.md: never push or open PRs unless the user explicitly asks (they have authorized
  pushes to their fork case by case).
- Follow docs/policies/AI.md (Assisted-by footers, no AI Co-authored-by) and
  docs/policies/Coding.md (no [[likely]]/[[unlikely]], etc.).
- Build with ccache; ccache max size is 100G. Baloo excludes ~/Projects.

## Update: all perf branches merged locally (not pushed)
perf/back-thread now = ca32ec64c5 profiler + 9c82b9487e write log + merges of perf/sync
(33cbfcf584), perf/draw (a80e07ec7b; nx2_profile counters resolved: acquire 8-11, uploads
12-15, array 16) and perf/ring (635dac27db; fixed in the merge: WaitForEffects flushes the
pending cb run first and counts Run entries as effects, otherwise a macro DMA read could miss
buffered constant buffer writes). Builds; tests 66/74 pass (memory_tracker x8 known).
Kill switches: EDEN_NX2_ACQUIRE_FASTPATH, EDEN_NX2_DMA_READ_WAITFOR, EDEN_NX2_DRAW_CACHE,
EDEN_NX2_SPLIT_BATCH, EDEN_NX2_SPLIT_CB_RUN (=0 disables). EDEN_NX2_SPLIT_VERIFY=1 checks the
front's register mirrors. Next: benchmark in game, then push/PR (ask the user).
port/zortos-fixes finished separately (e4b3a460f8 tests fix, 1b293b5050 Mario Tennis Fever;
bumps shader cache versions) - keep as its own PR.

## Update: Mario Kart World / PR #5 upstream import (merged into nx2)
- Curated branch `pick/upstream-mkw` (58 commits, ending at 2580f62ecf: vi2 pacing sleeps in guest kernel)
  pushed to origin/pick/upstream-mkw.
- Opened PR #6 on zoeleu/Eglantine and merged it into nx2 (merge commit 336a9337c4).
- PR #5 (Djihads80:nx2-experimental) closed on zoeleu/Eglantine as superseded by PR #6.
- Local nx2 fast-forwarded to origin/nx2 (336a9337c4).

