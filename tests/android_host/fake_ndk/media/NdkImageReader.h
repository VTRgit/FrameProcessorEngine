// Host-test stand-in for the NDK header of the same name (subset, same signatures).
#pragma once
#include <android/native_window.h>
#include <media/NdkImage.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct AImageReader AImageReader;
typedef void (*AImageReader_ImageCallback)(void* context, AImageReader* reader);
typedef struct AImageReader_ImageListener {
  void* context;
  AImageReader_ImageCallback onImageAvailable;
} AImageReader_ImageListener;
media_status_t AImageReader_newWithUsage(int32_t width, int32_t height, int32_t format, uint64_t usage,
                                         int32_t maxImages, AImageReader** reader);
void AImageReader_delete(AImageReader* reader);
media_status_t AImageReader_getWindow(AImageReader* reader, ANativeWindow** window);
media_status_t AImageReader_setImageListener(AImageReader* reader, AImageReader_ImageListener* listener);
media_status_t AImageReader_acquireLatestImage(AImageReader* reader, AImage** image);
#ifdef __cplusplus
}
#endif
