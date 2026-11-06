#include "fake_ndk.h"

#include <android/native_window_jni.h>

#include <cstring>

struct AImageReader {};
struct ANativeWindow {};

namespace {
AImageReader gReaderHandle;
ANativeWindow gWindow;
fakendk::Reader gReader;
} // namespace

namespace fakendk {

void makeYuv(AImage& img, std::uint8_t* memory, std::int32_t w, std::int32_t h, std::int64_t ts) {
  img.format = AIMAGE_FORMAT_YUV_420_888;
  img.width = w;
  img.height = h;
  img.timestampNs = ts;
  img.planeCount = 3;
  img.data[0] = memory;
  img.length[0] = w * h;
  img.rowStride[0] = w;
  img.pixelStride[0] = 1;
  img.data[1] = memory + w * h;     // U
  img.data[2] = memory + w * h + 1; // V, interleaved with U
  img.length[1] = img.length[2] = w * h / 2 - 1;
  img.rowStride[1] = img.rowStride[2] = w;
  img.pixelStride[1] = img.pixelStride[2] = 2;
  img.buffer = nullptr;
  for (std::int32_t i = 0; i < w * h; ++i) {
    memory[i] = static_cast<std::uint8_t>((i + ts / 1000) & 0xFF);
  }
}

Reader& reader() { return gReader; }

void resetReader() {
  for (auto& img : gReader.images) {
    img.deletes.store(0);
    img.inUse.store(false);
  }
  gReader.held.store(0);
  gReader.pendingIndex.store(-1);
  gReader.listener = {};
  gReader.deleted = false;
  gReader.created = 0;
  gReader.maxImages = 0;
}

bool produce() {
  Reader& r = gReader;
  // Replace a pending image that nobody acquired.
  int pending = r.pendingIndex.exchange(-1);
  if (pending >= 0) {
    r.images[pending].inUse.store(false);
  }
  int free = -1;
  for (int i = 0; i < r.maxImages && i < Reader::kMaxImages; ++i) {
    bool expected = false;
    if (r.images[i].inUse.compare_exchange_strong(expected, true)) {
      free = i;
      break;
    }
  }
  if (free < 0) {
    return false;
  }
  makeYuv(r.images[free], r.memory[free], r.width, r.height, r.nextTimestamp.fetch_add(33'333'333));
  r.pendingIndex.store(free);
  if (r.listener.onImageAvailable != nullptr) {
    r.listener.onImageAvailable(r.listener.context, &gReaderHandle);
  }
  return true;
}

} // namespace fakendk

extern "C" {

void AHardwareBuffer_acquire(AHardwareBuffer* b) { b->acquires.fetch_add(1); }
void AHardwareBuffer_release(AHardwareBuffer* b) { b->releases.fetch_add(1); }

void AImage_delete(AImage* image) {
  image->deletes.fetch_add(1);
  // Images owned by the fake reader go back to it.
  for (auto& img : gReader.images) {
    if (&img == image) {
      gReader.held.fetch_sub(1);
      img.inUse.store(false);
    }
  }
}
media_status_t AImage_getWidth(const AImage* i, int32_t* v) { *v = i->width; return AMEDIA_OK; }
media_status_t AImage_getHeight(const AImage* i, int32_t* v) { *v = i->height; return AMEDIA_OK; }
media_status_t AImage_getFormat(const AImage* i, int32_t* v) { *v = i->format; return AMEDIA_OK; }
media_status_t AImage_getTimestamp(const AImage* i, int64_t* v) { *v = i->timestampNs; return AMEDIA_OK; }
media_status_t AImage_getNumberOfPlanes(const AImage* i, int32_t* v) {
  if (i->format == AIMAGE_FORMAT_PRIVATE) {
    return AMEDIA_ERROR_INVALID_PARAMETER;
  }
  *v = i->planeCount;
  return AMEDIA_OK;
}
media_status_t AImage_getPlanePixelStride(const AImage* i, int p, int32_t* v) {
  *v = i->pixelStride[p];
  return AMEDIA_OK;
}
media_status_t AImage_getPlaneRowStride(const AImage* i, int p, int32_t* v) {
  *v = i->rowStride[p];
  return AMEDIA_OK;
}
media_status_t AImage_getPlaneData(const AImage* i, int p, uint8_t** d, int* len) {
  *d = i->data[p];
  *len = i->length[p];
  return AMEDIA_OK;
}
media_status_t AImage_getHardwareBuffer(const AImage* i, AHardwareBuffer** b) {
  if (i->buffer == nullptr) {
    return AMEDIA_ERROR_UNKNOWN;
  }
  *b = i->buffer;
  return AMEDIA_OK;
}

media_status_t AImageReader_newWithUsage(int32_t width, int32_t height, int32_t format, uint64_t,
                                         int32_t maxImages, AImageReader** reader) {
  if (maxImages <= 0 || maxImages > fakendk::Reader::kMaxImages || width * height * 2 > 64 * 48 * 2) {
    return AMEDIA_ERROR_INVALID_PARAMETER;
  }
  gReader.width = width;
  gReader.height = height;
  gReader.format = format;
  gReader.maxImages = maxImages;
  gReader.created += 1;
  *reader = &gReaderHandle;
  return AMEDIA_OK;
}
void AImageReader_delete(AImageReader*) { gReader.deleted = true; }
media_status_t AImageReader_getWindow(AImageReader*, ANativeWindow** w) {
  *w = &gWindow;
  return AMEDIA_OK;
}
media_status_t AImageReader_setImageListener(AImageReader*, AImageReader_ImageListener* l) {
  gReader.listener = *l;
  return AMEDIA_OK;
}
media_status_t AImageReader_acquireLatestImage(AImageReader*, AImage** image) {
  // Like the real reader: one image slot must stay free to swap in the newest one.
  if (gReader.held.load() >= gReader.maxImages - 1) {
    return AMEDIA_IMGREADER_MAX_IMAGES_ACQUIRED;
  }
  int idx = gReader.pendingIndex.exchange(-1);
  if (idx < 0) {
    return AMEDIA_IMGREADER_NO_BUFFER_AVAILABLE;
  }
  gReader.held.fetch_add(1);
  *image = &gReader.images[idx];
  return AMEDIA_OK;
}

jobject ANativeWindow_toSurface(JNIEnv*, ANativeWindow*) { return nullptr; }

} // extern "C"
