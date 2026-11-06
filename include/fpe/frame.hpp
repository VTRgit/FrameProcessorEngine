#pragma once

#include <fpe/status.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

namespace fpe {

namespace detail {
class IndexStack;
}

/// `YUV420` is flexible 4:2:0 (e.g. Android YUV_420_888): three planes whose row and pixel
/// strides describe the layout. `Opaque` frames may have no CPU planes, only `Planes::native`.
enum class PixelFormat : std::uint8_t { Opaque, RGBA8, BGRA8, NV12, NV21, Gray8, YUV420 };

/// Up to three CPU-visible planes, plus an optional platform buffer (e.g. `AHardwareBuffer*`,
/// `CVPixelBufferRef`) for tasks that work on the GPU.
struct Planes {
  std::uint8_t* data[3]{};
  std::uint32_t stride[3]{};      ///< Row stride in bytes.
  std::uint32_t pixelStride[3]{}; ///< Bytes between adjacent pixels of a row (0: packed/unknown).
  std::uint32_t count = 0;
  void* native = nullptr;
};

struct FrameInfo {
  std::uint32_t streamId = 0;
  std::uint64_t frameId = 0;
  std::int64_t timestampNs = 0; ///< Capture time, in the source's clock (e.g. the camera sensor's).
  std::int64_t receivedNs = 0;  ///< When the frame entered the engine (steady clock); set by submit/process if 0.
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  PixelFormat format = PixelFormat::Opaque;
};

/// 8-byte reference to a frame slot: index + generation. Trivially copyable; ownership is
/// tracked by the slot's reference count, not by the handle (use FrameRef for RAII).
struct FrameHandle {
  static constexpr std::uint32_t kInvalid = 0xFFFFFFFFu;
  std::uint32_t index = kInvalid;
  std::uint32_t generation = 0;
  constexpr bool valid() const noexcept { return index != kInvalid; }
  friend constexpr bool operator==(FrameHandle, FrameHandle) = default;
};
static_assert(sizeof(FrameHandle) == 8);
static_assert(std::is_trivially_copyable_v<FrameHandle>);

/// Called once when the last reference to an external frame is released. May run on any engine
/// thread; must not block for long.
using ReleaseFn = void (*)(void* context) noexcept;

struct FrameStoreConfig {
  std::uint32_t poolSlots = 4;      ///< Frames whose pixels the store owns.
  std::size_t poolSlotBytes = 0;    ///< Bytes per pool slot (0: pool slots carry no memory).
  std::uint32_t externalSlots = 4;  ///< Frames wrapping memory the store does not own.
  std::size_t alignment = 64;       ///< Alignment of each pool slot's memory.
};

/// Fixed table of frame slots. All memory is allocated in the constructor; acquire, wrap, retain
/// and release never allocate, never block and are safe from any thread.
class FrameStore {
public:
  explicit FrameStore(const FrameStoreConfig& config);
  ~FrameStore();
  FrameStore(const FrameStore&) = delete;
  FrameStore& operator=(const FrameStore&) = delete;

  /// Takes a free pool slot with one reference. Fill it through mutableInfo()/planes().
  Status acquire(FrameHandle& out) noexcept;

  /// Wraps external memory in a slot with one reference. `releaseFn` runs exactly once, when the
  /// last reference goes away. On failure (`PoolExhausted`) `releaseFn` is NOT called: the caller
  /// still owns the memory.
  Status wrapExternal(const FrameInfo& info, const Planes& planes, ReleaseFn releaseFn,
                      void* context, FrameHandle& out) noexcept;

  /// Adds a reference. The caller must already hold one.
  Status retain(FrameHandle h) noexcept;

  /// Drops a reference; the last one frees the slot (and calls the release callback).
  Status release(FrameHandle h) noexcept;

  /// Accessors; valid while the caller holds a reference. nullptr for stale handles.
  const FrameInfo* info(FrameHandle h) const noexcept;
  FrameInfo* mutableInfo(FrameHandle h) noexcept;
  const Planes* planes(FrameHandle h) const noexcept;
  Planes* mutablePlanes(FrameHandle h) noexcept;

  /// Owned memory of a pool slot (nullptr for external frames).
  std::uint8_t* poolMemory(FrameHandle h) const noexcept;
  std::size_t poolSlotBytes() const noexcept { return slotBytes_; }

  bool isLive(FrameHandle h) const noexcept;
  std::uint32_t poolCapacity() const noexcept { return poolSlots_; }
  std::uint32_t externalCapacity() const noexcept { return externalSlots_; }
  /// Slots currently referenced (approximate while other threads are working).
  std::uint32_t inUse() const noexcept { return inUse_.load(std::memory_order_relaxed); }

private:
  struct Slot;
  Slot* live(FrameHandle h) const noexcept;
  Status finishAcquire(std::uint32_t index, FrameHandle& out) noexcept;

  std::uint32_t poolSlots_;
  std::uint32_t externalSlots_;
  std::size_t slotBytes_;
  std::size_t alignment_;
  std::unique_ptr<Slot[]> slots_;
  std::uint8_t* memory_ = nullptr;
  std::unique_ptr<detail::IndexStack> poolFree_;
  std::unique_ptr<detail::IndexStack> externalFree_;
  std::atomic<std::uint32_t> inUse_{0};
};

/// RAII owner of one reference to a frame. Move-only; share() adds a reference.
class FrameRef {
public:
  FrameRef() noexcept = default;
  /// Adopts one reference the caller already holds.
  FrameRef(FrameStore& store, FrameHandle h) noexcept : store_(&store), handle_(h) {}
  FrameRef(FrameRef&& o) noexcept : store_(o.store_), handle_(o.handle_) { o.store_ = nullptr; }
  FrameRef& operator=(FrameRef&& o) noexcept {
    if (this != &o) {
      reset();
      store_ = o.store_;
      handle_ = o.handle_;
      o.store_ = nullptr;
    }
    return *this;
  }
  FrameRef(const FrameRef&) = delete;
  FrameRef& operator=(const FrameRef&) = delete;
  ~FrameRef() { reset(); }

  /// A second owner of the same frame.
  FrameRef share() const noexcept {
    if (store_ == nullptr || store_->retain(handle_) != Status::Ok) {
      return {};
    }
    return FrameRef(*store_, handle_);
  }

  void reset() noexcept {
    if (store_ != nullptr) {
      store_->release(handle_);
      store_ = nullptr;
    }
  }

  /// Gives up ownership without releasing; returns the handle (now owned by the caller).
  FrameHandle detach() noexcept {
    store_ = nullptr;
    return handle_;
  }

  explicit operator bool() const noexcept { return store_ != nullptr; }
  FrameHandle handle() const noexcept { return handle_; }
  FrameStore* store() const noexcept { return store_; }
  const FrameInfo* info() const noexcept { return store_ ? store_->info(handle_) : nullptr; }
  const Planes* planes() const noexcept { return store_ ? store_->planes(handle_) : nullptr; }

private:
  FrameStore* store_ = nullptr;
  FrameHandle handle_{};
};

} // namespace fpe
