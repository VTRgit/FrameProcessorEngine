#include <fpe/detail/index_stack.hpp>
#include <fpe/frame.hpp>

#include <new>

namespace fpe {

struct FrameStore::Slot {
  std::atomic<std::uint32_t> refs{0};
  std::atomic<std::uint32_t> generation{1};
  FrameInfo info{};
  Planes planes{};
  ReleaseFn release = nullptr;
  void* releaseContext = nullptr;
  std::uint8_t* memory = nullptr; // pool slots only
};

namespace {
std::size_t roundUp(std::size_t v, std::size_t a) { return (v + a - 1) / a * a; }
} // namespace

FrameStore::FrameStore(const FrameStoreConfig& config)
    : poolSlots_(config.poolSlots), externalSlots_(config.externalSlots),
      slotBytes_(config.poolSlotBytes), alignment_(config.alignment < alignof(std::max_align_t)
                                                       ? alignof(std::max_align_t)
                                                       : config.alignment),
      slots_(std::make_unique<Slot[]>(std::size_t{config.poolSlots} + config.externalSlots)),
      poolFree_(std::make_unique<detail::IndexStack>(config.poolSlots + config.externalSlots)),
      externalFree_(std::make_unique<detail::IndexStack>(config.poolSlots + config.externalSlots)) {
  if (slotBytes_ > 0 && poolSlots_ > 0) {
    const std::size_t stride = roundUp(slotBytes_, alignment_);
    memory_ = static_cast<std::uint8_t*>(
        ::operator new(stride * poolSlots_, std::align_val_t{alignment_}));
    for (std::uint32_t i = 0; i < poolSlots_; ++i) {
      slots_[i].memory = memory_ + stride * i;
    }
  }
  // Push in reverse so that the first acquire returns index 0 (easier to read in tests).
  for (std::uint32_t i = poolSlots_; i-- > 0;) {
    poolFree_->push(i);
  }
  for (std::uint32_t i = poolSlots_ + externalSlots_; i-- > poolSlots_;) {
    externalFree_->push(i);
  }
}

FrameStore::~FrameStore() {
  if (memory_ != nullptr) {
    ::operator delete(memory_, std::align_val_t{alignment_});
  }
}

Status FrameStore::finishAcquire(std::uint32_t index, FrameHandle& out) noexcept {
  Slot& s = slots_[index];
  s.refs.store(1, std::memory_order_relaxed);
  inUse_.fetch_add(1, std::memory_order_relaxed);
  out = FrameHandle{index, s.generation.load(std::memory_order_relaxed)};
  return Status::Ok;
}

Status FrameStore::acquire(FrameHandle& out) noexcept {
  const std::uint32_t index = poolFree_->pop();
  if (index == detail::IndexStack::npos) {
    out = {};
    return Status::PoolExhausted;
  }
  Slot& s = slots_[index];
  s.info = FrameInfo{};
  s.planes = Planes{};
  if (s.memory != nullptr) {
    s.planes.data[0] = s.memory;
    s.planes.count = 1;
  }
  s.release = nullptr;
  s.releaseContext = nullptr;
  return finishAcquire(index, out);
}

Status FrameStore::wrapExternal(const FrameInfo& info, const Planes& planes, ReleaseFn releaseFn,
                                void* context, FrameHandle& out) noexcept {
  if (planes.count > 3) {
    out = {};
    return Status::InvalidArgument;
  }
  const std::uint32_t index = externalFree_->pop();
  if (index == detail::IndexStack::npos) {
    out = {};
    return Status::PoolExhausted;
  }
  Slot& s = slots_[index];
  s.info = info;
  s.planes = planes;
  s.release = releaseFn;
  s.releaseContext = context;
  return finishAcquire(index, out);
}

FrameStore::Slot* FrameStore::live(FrameHandle h) const noexcept {
  if (h.index >= poolSlots_ + externalSlots_) {
    FPE_ASSERT(false, "frame handle out of range");
    return nullptr;
  }
  Slot& s = slots_[h.index];
  if (s.generation.load(std::memory_order_acquire) != h.generation ||
      s.refs.load(std::memory_order_relaxed) == 0) {
    FPE_ASSERT(false, "stale frame handle");
    return nullptr;
  }
  return &s;
}

bool FrameStore::isLive(FrameHandle h) const noexcept {
  if (h.index >= poolSlots_ + externalSlots_) {
    return false;
  }
  const Slot& s = slots_[h.index];
  return s.generation.load(std::memory_order_acquire) == h.generation &&
         s.refs.load(std::memory_order_relaxed) != 0;
}

Status FrameStore::retain(FrameHandle h) noexcept {
  Slot* s = live(h);
  if (s == nullptr) {
    return Status::StaleHandle;
  }
  s->refs.fetch_add(1, std::memory_order_relaxed);
  return Status::Ok;
}

Status FrameStore::release(FrameHandle h) noexcept {
  Slot* s = live(h);
  if (s == nullptr) {
    return Status::StaleHandle;
  }
  if (s->refs.fetch_sub(1, std::memory_order_acq_rel) != 1) {
    return Status::Ok;
  }
  // Last reference: invalidate outstanding handles, then hand the slot back.
  s->generation.fetch_add(1, std::memory_order_release);
  if (s->release != nullptr) {
    s->release(s->releaseContext);
  }
  inUse_.fetch_sub(1, std::memory_order_relaxed);
  if (h.index < poolSlots_) {
    poolFree_->push(h.index);
  } else {
    externalFree_->push(h.index);
  }
  return Status::Ok;
}

const FrameInfo* FrameStore::info(FrameHandle h) const noexcept {
  const Slot* s = live(h);
  return s ? &s->info : nullptr;
}

FrameInfo* FrameStore::mutableInfo(FrameHandle h) noexcept {
  Slot* s = live(h);
  return s ? &s->info : nullptr;
}

const Planes* FrameStore::planes(FrameHandle h) const noexcept {
  const Slot* s = live(h);
  return s ? &s->planes : nullptr;
}

Planes* FrameStore::mutablePlanes(FrameHandle h) noexcept {
  Slot* s = live(h);
  return s ? &s->planes : nullptr;
}

std::uint8_t* FrameStore::poolMemory(FrameHandle h) const noexcept {
  const Slot* s = live(h);
  return s ? s->memory : nullptr;
}

} // namespace fpe
