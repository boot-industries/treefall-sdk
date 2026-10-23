// SPDX-License-Identifier: MIT

#include "rt_guard.hpp"

#include <cstdlib>
#include <new>

namespace orpheus::tests::support {

thread_local int RtGuardState::t_sectionDepth = 0;
std::atomic<std::uint64_t> RtGuardState::s_allocViolations{0};
std::atomic<std::uint64_t> RtGuardState::s_deallocViolations{0};
std::atomic<std::uint64_t> RtGuardState::s_allocViolationBytes{0};

} // namespace orpheus::tests::support

namespace {

void* guardedAlloc(std::size_t size) {
  using orpheus::tests::support::RtGuardState;
  if (RtGuardState::t_sectionDepth > 0) {
    RtGuardState::s_allocViolations.fetch_add(1, std::memory_order_relaxed);
    RtGuardState::s_allocViolationBytes.fetch_add(size, std::memory_order_relaxed);
  }
  return std::malloc(size);
}

void guardedFree(void* ptr) {
  using orpheus::tests::support::RtGuardState;
  if (ptr != nullptr && RtGuardState::t_sectionDepth > 0) {
    RtGuardState::s_deallocViolations.fetch_add(1, std::memory_order_relaxed);
  }
  std::free(ptr);
}

} // namespace

void* operator new(std::size_t size) {
  void* ptr = guardedAlloc(size);
  if (ptr == nullptr) {
    throw std::bad_alloc();
  }
  return ptr;
}

void* operator new[](std::size_t size) {
  return ::operator new(size);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
  return guardedAlloc(size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
  return guardedAlloc(size);
}

void operator delete(void* ptr) noexcept {
  guardedFree(ptr);
}

void operator delete[](void* ptr) noexcept {
  guardedFree(ptr);
}

void operator delete(void* ptr, std::size_t) noexcept {
  guardedFree(ptr);
}

void operator delete[](void* ptr, std::size_t) noexcept {
  guardedFree(ptr);
}

void operator delete(void* ptr, const std::nothrow_t&) noexcept {
  guardedFree(ptr);
}

void operator delete[](void* ptr, const std::nothrow_t&) noexcept {
  guardedFree(ptr);
}
