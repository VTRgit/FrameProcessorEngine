// Host-test stand-in for the NDK header of the same name (subset, same values and signatures).
#pragma once
typedef enum {
  AMEDIA_OK = 0,
  AMEDIA_ERROR_UNKNOWN = -10000,
  AMEDIA_ERROR_INVALID_PARAMETER = -10007,
  AMEDIA_IMGREADER_NO_BUFFER_AVAILABLE = -30001,
  AMEDIA_IMGREADER_MAX_IMAGES_ACQUIRED = -30002,
} media_status_t;
