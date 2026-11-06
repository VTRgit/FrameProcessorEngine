// Host-test stand-in for the NDK header of the same name (subset, same values and signatures).
#pragma once
#include <android/hardware_buffer.h>
#include <media/NdkMediaError.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct AImage AImage;
enum AIMAGE_FORMATS {
  AIMAGE_FORMAT_RGBA_8888 = 0x1,
  AIMAGE_FORMAT_PRIVATE = 0x22,
  AIMAGE_FORMAT_YUV_420_888 = 0x23,
  AIMAGE_FORMAT_JPEG = 0x100,
};
void AImage_delete(AImage* image);
media_status_t AImage_getWidth(const AImage* image, int32_t* width);
media_status_t AImage_getHeight(const AImage* image, int32_t* height);
media_status_t AImage_getFormat(const AImage* image, int32_t* format);
media_status_t AImage_getTimestamp(const AImage* image, int64_t* timestampNs);
media_status_t AImage_getNumberOfPlanes(const AImage* image, int32_t* numPlanes);
media_status_t AImage_getPlanePixelStride(const AImage* image, int planeIdx, int32_t* pixelStride);
media_status_t AImage_getPlaneRowStride(const AImage* image, int planeIdx, int32_t* rowStride);
media_status_t AImage_getPlaneData(const AImage* image, int planeIdx, uint8_t** data, int* dataLength);
media_status_t AImage_getHardwareBuffer(const AImage* image, AHardwareBuffer** buffer);
#ifdef __cplusplus
}
#endif
