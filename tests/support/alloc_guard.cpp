#include "alloc_guard.h"

#include <atomic>
#include <cstdlib>
#include <new>

namespace {

std::atomic<std::int64_t> gCount{0};
thread_local bool tArmed = false;

#if FPE_TEST_ALLOC_GUARD
inline void note() noexcept {
  if (tArmed) {
    gCount.fetch_add(1, std::memory_order_relaxed);
  }
}

void* allocate(std::size_t size) {
  note();
  if (size == 0) {
    size = 1;
  }
  void* p = std::malloc(size);
  if (p == nullptr) {
    throw std::bad_alloc();
  }
  return p;
}

void* allocateAligned(std::size_t size, std::align_val_t align) {
  note();
  auto alignment = static_cast<std::size_t>(align);
  if (alignment < sizeof(void*)) {
    alignment = sizeof(void*);
  }
  std::size_t rounded = (size + alignment - 1) / alignment * alignment;
  if (rounded == 0) {
    rounded = alignment;
  }
  void* p = nullptr;
  if (posix_memalign(&p, alignment, rounded) != 0) {
    throw std::bad_alloc();
  }
  return p;
}

void deallocate(void* p) noexcept {
  if (p != nullptr) {
    note();
  }
  std::free(p);
}
#endif

} // namespace

namespace fpe::test {

bool allocGuardAvailable() noexcept { return FPE_TEST_ALLOC_GUARD != 0; }

void armCurrentThread() noexcept { tArmed = true; }
void disarmCurrentThread() noexcept { tArmed = false; }
std::int64_t allocationCount() noexcept { return gCount.load(std::memory_order_relaxed); }
void resetAllocationCount() noexcept { gCount.store(0, std::memory_order_relaxed); }

AllocGuard::AllocGuard() noexcept : start_(allocationCount()), wasArmed_(tArmed) { tArmed = true; }
AllocGuard::~AllocGuard() { tArmed = wasArmed_; }
std::int64_t AllocGuard::count() const noexcept { return allocationCount() - start_; }

} // namespace fpe::test

#if FPE_TEST_ALLOC_GUARD
// Global replacements. Only linked into the test executable.
void* operator new(std::size_t size) { return allocate(size); }
void* operator new[](std::size_t size) { return allocate(size); }
void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
  try {
    return allocate(size);
  } catch (...) {
    return nullptr;
  }
}
void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
  try {
    return allocate(size);
  } catch (...) {
    return nullptr;
  }
}
void* operator new(std::size_t size, std::align_val_t a) { return allocateAligned(size, a); }
void* operator new[](std::size_t size, std::align_val_t a) { return allocateAligned(size, a); }
void* operator new(std::size_t size, std::align_val_t a, const std::nothrow_t&) noexcept {
  try {
    return allocateAligned(size, a);
  } catch (...) {
    return nullptr;
  }
}
void* operator new[](std::size_t size, std::align_val_t a, const std::nothrow_t&) noexcept {
  try {
    return allocateAligned(size, a);
  } catch (...) {
    return nullptr;
  }
}

void operator delete(void* p) noexcept { deallocate(p); }
void operator delete[](void* p) noexcept { deallocate(p); }
void operator delete(void* p, std::size_t) noexcept { deallocate(p); }
void operator delete[](void* p, std::size_t) noexcept { deallocate(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { deallocate(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { deallocate(p); }
void operator delete(void* p, std::align_val_t) noexcept { deallocate(p); }
void operator delete[](void* p, std::align_val_t) noexcept { deallocate(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { deallocate(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { deallocate(p); }
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept { deallocate(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept { deallocate(p); }
#endif // FPE_TEST_ALLOC_GUARD
