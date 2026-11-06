#include <fpe/android/camera.hpp>

#include <android/native_window_jni.h>

namespace fpe::android {

namespace {

void deleteImage(void* context) noexcept { AImage_delete(static_cast<AImage*>(context)); }
void releaseBuffer(void* context) noexcept {
  AHardwareBuffer_release(static_cast<AHardwareBuffer*>(context));
}

std::uint32_t u32(std::int32_t v) noexcept { return v > 0 ? static_cast<std::uint32_t>(v) : 0u; }

} // namespace

Status describeImage(const AImage* image, std::uint32_t streamId, std::uint64_t frameId,
                     FrameInfo& info, Planes& planes) noexcept {
  if (image == nullptr) {
    return Status::InvalidArgument;
  }
  std::int32_t format = 0, width = 0, height = 0;
  std::int64_t timestamp = 0;
  if (AImage_getFormat(image, &format) != AMEDIA_OK || AImage_getWidth(image, &width) != AMEDIA_OK ||
      AImage_getHeight(image, &height) != AMEDIA_OK ||
      AImage_getTimestamp(image, &timestamp) != AMEDIA_OK) {
    return Status::InvalidArgument;
  }
  info = FrameInfo{};
  info.streamId = streamId;
  info.frameId = frameId;
  info.timestampNs = timestamp;
  info.width = u32(width);
  info.height = u32(height);
  planes = Planes{};

  std::int32_t expectedPlanes = 0;
  switch (format) {
  case AIMAGE_FORMAT_YUV_420_888:
    info.format = PixelFormat::YUV420;
    expectedPlanes = 3;
    break;
  case AIMAGE_FORMAT_RGBA_8888:
    info.format = PixelFormat::RGBA8;
    expectedPlanes = 1;
    break;
  case AIMAGE_FORMAT_PRIVATE: {
    info.format = PixelFormat::Opaque;
    AHardwareBuffer* buffer = nullptr;
    if (AImage_getHardwareBuffer(image, &buffer) != AMEDIA_OK || buffer == nullptr) {
      return Status::InvalidArgument;
    }
    planes.native = buffer;
    return Status::Ok;
  }
  default:
    return Status::InvalidArgument;
  }

  std::int32_t count = 0;
  if (AImage_getNumberOfPlanes(image, &count) != AMEDIA_OK || count != expectedPlanes) {
    return Status::InvalidArgument;
  }
  for (int p = 0; p < count; ++p) {
    std::uint8_t* data = nullptr;
    int length = 0;
    std::int32_t rowStride = 0, pixelStride = 0;
    if (AImage_getPlaneData(image, p, &data, &length) != AMEDIA_OK ||
        AImage_getPlaneRowStride(image, p, &rowStride) != AMEDIA_OK ||
        AImage_getPlanePixelStride(image, p, &pixelStride) != AMEDIA_OK || data == nullptr) {
      return Status::InvalidArgument;
    }
    const auto i = static_cast<std::size_t>(p);
    planes.data[i] = data;
    planes.stride[i] = u32(rowStride);
    planes.pixelStride[i] = u32(pixelStride);
  }
  planes.count = static_cast<std::uint32_t>(count);
  // Hardware buffer too, when the image has one (GPU tasks can import it).
  AHardwareBuffer* buffer = nullptr;
  if (AImage_getHardwareBuffer(image, &buffer) == AMEDIA_OK) {
    planes.native = buffer;
  }
  return Status::Ok;
}

Status wrapAImage(FrameStore& store, AImage* image, std::uint32_t streamId, std::uint64_t frameId,
                  FrameRef& out) noexcept {
  FrameInfo info;
  Planes planes;
  if (Status s = describeImage(image, streamId, frameId, info, planes); s != Status::Ok) {
    return s;
  }
  FrameHandle h;
  if (Status s = store.wrapExternal(info, planes, &deleteImage, image, h); s != Status::Ok) {
    return s;
  }
  out = FrameRef(store, h);
  return Status::Ok;
}

Status wrapHardwareBuffer(FrameStore& store, AHardwareBuffer* buffer, const FrameInfo& info,
                          FrameRef& out) noexcept {
  if (buffer == nullptr) {
    return Status::InvalidArgument;
  }
  Planes planes;
  planes.native = buffer;
  FrameHandle h;
  if (Status s = store.wrapExternal(info, planes, &releaseBuffer, buffer, h); s != Status::Ok) {
    return s;
  }
  AHardwareBuffer_acquire(buffer);
  out = FrameRef(store, h);
  return Status::Ok;
}

// ---- ImageReaderSource -----------------------------------------------------------------------

ImageReaderSource::ImageReaderSource(FrameStore& store, Engine& engine, const ImageReaderConfig& config)
    : store_(store), engine_(engine), config_(config),
      slots_(std::make_unique<Slot[]>(config.maxFramesInFlight + 2)),
      freeSlots_(config.maxFramesInFlight + 2) {
  for (std::uint32_t i = config.maxFramesInFlight + 2; i-- > 0;) {
    slots_[i].source = this;
    slots_[i].index = i;
    freeSlots_.push(i);
  }
}

ImageReaderSource::~ImageReaderSource() {
  stop();
  FPE_ASSERT(outstanding() == 0, "ImageReaderSource destroyed with images still referenced; "
                                 "stop the engine first");
  if (reader_ != nullptr) {
    AImageReader_delete(reader_);
  }
}

Status ImageReaderSource::open() noexcept {
  if (reader_ != nullptr) {
    return Status::AlreadyStarted;
  }
  const auto maxImages = static_cast<std::int32_t>(config_.maxFramesInFlight + 2);
  if (AImageReader_newWithUsage(config_.width, config_.height, config_.format, config_.usage,
                                maxImages, &reader_) != AMEDIA_OK ||
      reader_ == nullptr) {
    reader_ = nullptr;
    return Status::InvalidArgument;
  }
  if (AImageReader_getWindow(reader_, &window_) != AMEDIA_OK) {
    AImageReader_delete(reader_);
    reader_ = nullptr;
    return Status::InvalidArgument;
  }
  return Status::Ok;
}

Status ImageReaderSource::start() noexcept {
  if (reader_ == nullptr) {
    return Status::NotStarted;
  }
  listener_.context = this;
  listener_.onImageAvailable = &ImageReaderSource::onImageAvailable;
  return AImageReader_setImageListener(reader_, &listener_) == AMEDIA_OK ? Status::Ok
                                                                          : Status::InvalidArgument;
}

void ImageReaderSource::stop() noexcept {
  if (reader_ != nullptr) {
    AImageReader_ImageListener none{nullptr, nullptr};
    AImageReader_setImageListener(reader_, &none);
  }
}

jobject ImageReaderSource::surface(JNIEnv* env) const noexcept {
  return window_ != nullptr ? ANativeWindow_toSurface(env, window_) : nullptr;
}

ImageReaderStats ImageReaderSource::stats() const noexcept {
  ImageReaderStats s;
  s.images = images_.load(std::memory_order_relaxed);
  s.acquireFailures = acquireFailures_.load(std::memory_order_relaxed);
  s.wrapFailures = wrapFailures_.load(std::memory_order_relaxed);
  s.rejected = rejected_.load(std::memory_order_relaxed);
  return s;
}

void ImageReaderSource::onImageAvailable(void* context, AImageReader*) {
  static_cast<ImageReaderSource*>(context)->handleImage();
}

void ImageReaderSource::releaseImage(void* context) noexcept {
  auto* slot = static_cast<Slot*>(context);
  ImageReaderSource* self = slot->source;
  AImage_delete(slot->image);
  slot->image = nullptr;
  self->freeSlots_.push(slot->index);
  self->outstanding_.fetch_sub(1, std::memory_order_acq_rel);
}

void ImageReaderSource::handleImage() noexcept {
  AImage* image = nullptr;
  if (AImageReader_acquireLatestImage(reader_, &image) != AMEDIA_OK || image == nullptr) {
    acquireFailures_.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  const std::uint32_t index = freeSlots_.pop();
  if (index == detail::IndexStack::npos) {
    AImage_delete(image);
    wrapFailures_.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  FrameInfo info;
  Planes planes;
  if (describeImage(image, config_.stream, nextFrameId_++, info, planes) != Status::Ok) {
    AImage_delete(image);
    freeSlots_.push(index);
    wrapFailures_.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  Slot& slot = slots_[index];
  slot.image = image;
  FrameHandle h;
  if (store_.wrapExternal(info, planes, &ImageReaderSource::releaseImage, &slot, h) != Status::Ok) {
    slot.image = nullptr;
    AImage_delete(image);
    freeSlots_.push(index);
    wrapFailures_.fetch_add(1, std::memory_order_relaxed);
    return;
  }
  outstanding_.fetch_add(1, std::memory_order_acq_rel);
  if (engine_.submit(config_.stream, FrameRef(store_, h)) != Status::Ok) {
    rejected_.fetch_add(1, std::memory_order_relaxed); // the FrameRef released the image
    return;
  }
  images_.fetch_add(1, std::memory_order_relaxed);
}

} // namespace fpe::android
