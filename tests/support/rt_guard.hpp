// SPDX-License-Identifier: MIT
#pragma once

// ORP136 §2.2: runtime realtime-safety harness support.
//
// A static grep (tools/realtime_audit.py) can prove a forbidden token is
// absent from a callback's source; it cannot prove the absence of allocation
// or blocking I/O at runtime. This header provides the runtime side:
//
//  * RtSection — RAII marker for "this thread is now inside an audio
//    callback". Thread-local, nestable.
//  * Allocation hooks — guarded test binaries link tests/support/rt_guard.cpp
//    to install global operator new/delete overrides. Any C++ allocation or
//    deallocation performed while the calling thread is inside an RtSection
//    is counted as a violation (the allocation itself still succeeds, so the
//    system under test keeps running and the test can report precisely).
//  * ProcIoCounters — Linux /proc/self/io sampling (read syscalls + bytes)
//    so a test can prove a guarded render section performed file I/O — or
//    prove that it did not. Returns has_value()==false where unsupported.
//
// Used by tests/transport/realtime_harness_test.cpp (the ORP134 G1 gate:
// "zero allocations/locks/blocking calls across a multi-clip processAudio()
// stress run") and reusable by future ORP136 gates.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <new>
#include <optional>
#include <string>

namespace orpheus::tests::support {

struct RtGuardState {
  // Depth of nested RtSections on this thread (0 == not in a callback).
  static thread_local int t_sectionDepth;

  // Global violation counters (atomic: violations from any guarded thread).
  static std::atomic<std::uint64_t> s_allocViolations;
  static std::atomic<std::uint64_t> s_deallocViolations;
  static std::atomic<std::uint64_t> s_allocViolationBytes;

  static void reset() {
    s_allocViolations.store(0, std::memory_order_relaxed);
    s_deallocViolations.store(0, std::memory_order_relaxed);
    s_allocViolationBytes.store(0, std::memory_order_relaxed);
  }

  static std::uint64_t allocViolations() {
    return s_allocViolations.load(std::memory_order_relaxed);
  }
  static std::uint64_t deallocViolations() {
    return s_deallocViolations.load(std::memory_order_relaxed);
  }
  static std::uint64_t allocViolationBytes() {
    return s_allocViolationBytes.load(std::memory_order_relaxed);
  }
  static std::uint64_t totalViolations() {
    return allocViolations() + deallocViolations();
  }
};

/// RAII: marks the current thread as executing an audio callback.
class RtSection {
public:
  RtSection() {
    ++RtGuardState::t_sectionDepth;
  }
  ~RtSection() {
    --RtGuardState::t_sectionDepth;
  }
  RtSection(const RtSection&) = delete;
  RtSection& operator=(const RtSection&) = delete;
};

inline bool inRtSection() {
  return RtGuardState::t_sectionDepth > 0;
}

/// Linux /proc/self/io snapshot. syscr = read() syscall count, rchar = bytes
/// requested from read-family syscalls (counted even when served from the
/// page cache — which freshly-written test fixtures always are, so this is
/// the right counter for "did the render section hit the filesystem").
struct ProcIoCounters {
  std::uint64_t syscr = 0;
  std::uint64_t rchar = 0;
};

inline std::optional<ProcIoCounters> readProcIoCounters() {
#if defined(__linux__)
  std::ifstream io("/proc/self/io");
  if (!io.is_open()) {
    return std::nullopt;
  }
  ProcIoCounters counters;
  std::string key;
  std::uint64_t value = 0;
  bool sawSyscr = false;
  bool sawRchar = false;
  while (io >> key >> value) {
    if (key == "syscr:") {
      counters.syscr = value;
      sawSyscr = true;
    } else if (key == "rchar:") {
      counters.rchar = value;
      sawRchar = true;
    }
  }
  if (!sawSyscr || !sawRchar) {
    return std::nullopt;
  }
  return counters;
#else
  return std::nullopt;
#endif
}

} // namespace orpheus::tests::support
