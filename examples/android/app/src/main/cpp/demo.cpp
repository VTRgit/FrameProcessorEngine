// FrameProcessorEngine camera demo: NDK Camera2 (back camera, 640x480 YUV) renders into an
// fpe::android::ImageReaderSource; two tasks run per frame; stats are read by the Kotlin UI.

#include <fpe/android/camera.hpp>
#include <fpe/engine.hpp>

#include <android/log.h>
#include <camera/NdkCameraCaptureSession.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadata.h>
#include <camera/NdkCaptureRequest.h>
#include <jni.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <memory>
#include <string>

#define LOG(...) __android_log_print(ANDROID_LOG_INFO, "FpeCameraDemo", __VA_ARGS__)

namespace {

constexpr std::int32_t kWidth = 640;
constexpr std::int32_t kHeight = 480;
constexpr std::uint32_t kFramesInFlight = 3;
constexpr int kThumbW = 32;
constexpr int kThumbH = 24;

std::int64_t nowNs(clockid_t clock) {
  timespec ts{};
  clock_gettime(clock, &ts);
  return static_cast<std::int64_t>(ts.tv_sec) * 1'000'000'000 + ts.tv_nsec;
}

struct Luma {
  float mean = 0;
};
struct Motion {
  float energy = 0; // mean absolute difference of a 32x24 thumbnail vs the previous frame, 0..1
};

/// Motion keeps the previous thumbnail. A stream has one frame in flight at a time, so frames
/// run this task one after another (on any worker), ordered by the engine.
struct MotionState {
  std::uint8_t previous[kThumbW * kThumbH]{};
  bool hasPrevious = false;
};

void thumbnail(const fpe::TaskInput& in, std::uint8_t* out) {
  const fpe::Planes& p = in.planes();
  const fpe::FrameInfo& info = in.info();
  for (int ty = 0; ty < kThumbH; ++ty) {
    const std::uint32_t y = static_cast<std::uint32_t>(ty) * info.height / kThumbH;
    for (int tx = 0; tx < kThumbW; ++tx) {
      const std::uint32_t x = static_cast<std::uint32_t>(tx) * info.width / kThumbW;
      out[ty * kThumbW + tx] = p.data[0][y * p.stride[0] + x * p.pixelStride[0]];
    }
  }
}

struct Demo {
  std::unique_ptr<fpe::FrameStore> store;
  std::unique_ptr<fpe::Engine> engine;
  std::unique_ptr<fpe::android::ImageReaderSource> source;
  MotionState motion;
  fpe::TaskId lumaTask = fpe::kInvalidTask;
  fpe::TaskId motionTask = fpe::kInvalidTask;
  clockid_t sensorClock = CLOCK_MONOTONIC;

  // Written by the result callback (worker threads), read by the UI.
  std::atomic<std::uint64_t> results{0};
  std::atomic<float> latencyMs{0};
  std::atomic<float> luma{0};
  std::atomic<float> motionEnergy{0};

  // UI thread only.
  std::uint64_t lastResults = 0;
  std::int64_t lastStatsNs = 0;
  float fps = 0;

  ACameraManager* manager = nullptr;
  ACameraDevice* device = nullptr;
  ACaptureSessionOutputContainer* outputs = nullptr;
  ACaptureSessionOutput* output = nullptr;
  ACameraOutputTarget* target = nullptr;
  ACaptureRequest* request = nullptr;
  ACameraCaptureSession* session = nullptr;
};

Demo* gDemo = nullptr;

void onResult(fpe::FrameResult&& r, void* ctx) noexcept {
  auto* d = static_cast<Demo*>(ctx);
  const float latency =
      static_cast<float>(nowNs(d->sensorClock) - r.info().timestampNs) / 1e6f;
  const float prev = d->latencyMs.load(std::memory_order_relaxed);
  d->latencyMs.store(prev == 0 ? latency : prev * 0.9f + latency * 0.1f, std::memory_order_relaxed);
  if (const Luma* l = r.get<Luma>(d->lumaTask)) {
    d->luma.store(l->mean, std::memory_order_relaxed);
  }
  if (const Motion* m = r.get<Motion>(d->motionTask)) {
    d->motionEnergy.store(m->energy, std::memory_order_relaxed);
  }
  d->results.fetch_add(1, std::memory_order_relaxed);
}

void onDisconnected(void*, ACameraDevice*) { LOG("camera disconnected"); }
void onError(void*, ACameraDevice*, int error) { LOG("camera error %d", error); }
void onSessionClosed(void*, ACameraCaptureSession*) {}
void onSessionReady(void*, ACameraCaptureSession*) {}
void onSessionActive(void*, ACameraCaptureSession*) {}

/// Opens the first back camera and streams into `window`. Returns an error or nullptr.
const char* openCamera(Demo& d, ANativeWindow* window) {
  d.manager = ACameraManager_create();
  ACameraIdList* ids = nullptr;
  if (ACameraManager_getCameraIdList(d.manager, &ids) != ACAMERA_OK || ids == nullptr) {
    return "no camera list";
  }
  const char* chosen = nullptr;
  for (int i = 0; i < ids->numCameras && chosen == nullptr; ++i) {
    ACameraMetadata* chars = nullptr;
    if (ACameraManager_getCameraCharacteristics(d.manager, ids->cameraIds[i], &chars) != ACAMERA_OK) {
      continue;
    }
    ACameraMetadata_const_entry facing{};
    if (ACameraMetadata_getConstEntry(chars, ACAMERA_LENS_FACING, &facing) == ACAMERA_OK &&
        facing.data.u8[0] == ACAMERA_LENS_FACING_BACK) {
      chosen = ids->cameraIds[i];
      ACameraMetadata_const_entry source{};
      if (ACameraMetadata_getConstEntry(chars, ACAMERA_SENSOR_INFO_TIMESTAMP_SOURCE, &source) ==
              ACAMERA_OK &&
          source.data.u8[0] == ACAMERA_SENSOR_INFO_TIMESTAMP_SOURCE_REALTIME) {
        d.sensorClock = CLOCK_BOOTTIME;
      }
    }
    ACameraMetadata_free(chars);
  }
  if (chosen == nullptr) {
    ACameraManager_deleteCameraIdList(ids);
    return "no back camera";
  }
  static ACameraDevice_StateCallbacks deviceCallbacks{nullptr, onDisconnected, onError};
  const camera_status_t opened = ACameraManager_openCamera(d.manager, chosen, &deviceCallbacks, &d.device);
  ACameraManager_deleteCameraIdList(ids);
  if (opened != ACAMERA_OK) {
    return "cannot open camera";
  }
  ACaptureSessionOutputContainer_create(&d.outputs);
  ACaptureSessionOutput_create(window, &d.output);
  ACaptureSessionOutputContainer_add(d.outputs, d.output);
  ACameraOutputTarget_create(window, &d.target);
  ACameraDevice_createCaptureRequest(d.device, TEMPLATE_PREVIEW, &d.request);
  ACaptureRequest_addTarget(d.request, d.target);
  static ACameraCaptureSession_stateCallbacks sessionCallbacks{nullptr, onSessionClosed,
                                                               onSessionReady, onSessionActive};
  if (ACameraDevice_createCaptureSession(d.device, d.outputs, &sessionCallbacks, &d.session) !=
      ACAMERA_OK) {
    return "cannot create capture session";
  }
  if (ACameraCaptureSession_setRepeatingRequest(d.session, nullptr, 1, &d.request, nullptr) !=
      ACAMERA_OK) {
    return "cannot start repeating request";
  }
  return nullptr;
}

void closeCamera(Demo& d) {
  if (d.session != nullptr) {
    ACameraCaptureSession_stopRepeating(d.session);
    ACameraCaptureSession_close(d.session);
  }
  if (d.request != nullptr) ACaptureRequest_free(d.request);
  if (d.target != nullptr) ACameraOutputTarget_free(d.target);
  if (d.outputs != nullptr) ACaptureSessionOutputContainer_free(d.outputs);
  if (d.output != nullptr) ACaptureSessionOutput_free(d.output);
  if (d.device != nullptr) ACameraDevice_close(d.device);
  if (d.manager != nullptr) ACameraManager_delete(d.manager);
  d.session = nullptr;
  d.request = nullptr;
  d.target = nullptr;
  d.outputs = nullptr;
  d.output = nullptr;
  d.device = nullptr;
  d.manager = nullptr;
}

void shutdown(Demo* d) {
  if (d == nullptr) return;
  closeCamera(*d);              // 1. no more images
  if (d->source) d->source->stop();
  if (d->engine) d->engine->stop(); // 2. every frame released
  d->source.reset();            // 3. now the reader can go
  d->engine.reset();
  d->store.reset();
  delete d;
}

} // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_com_vtrsoftware_fpe_camerademo_NativeDemo_start(JNIEnv* env, jobject) {
  if (gDemo != nullptr) {
    return nullptr;
  }
  auto* d = new Demo();
  d->store = std::make_unique<fpe::FrameStore>(
      fpe::FrameStoreConfig{.poolSlots = 0, .externalSlots = kFramesInFlight + 2});
  d->engine = std::make_unique<fpe::Engine>(
      *d->store, fpe::EngineConfig{.workers = 2, .streams = 1, .maxFramesInFlight = kFramesInFlight,
                                   .onResult = {&onResult, d}});
  d->lumaTask = d->engine->addTask<Luma>("luma", [](const fpe::TaskInput& in, Luma& out) {
    const fpe::Planes& p = in.planes();
    const fpe::FrameInfo& info = in.info();
    std::uint64_t sum = 0, n = 0;
    for (std::uint32_t y = 0; y < info.height; y += 4) {
      for (std::uint32_t x = 0; x < info.width; x += 4) {
        sum += p.data[0][y * p.stride[0] + x * p.pixelStride[0]];
        ++n;
      }
    }
    out.mean = n ? static_cast<float>(sum) / static_cast<float>(n) : 0.f;
  });
  MotionState* state = &d->motion;
  d->motionTask = d->engine->addTask<Motion>("motion", [state](const fpe::TaskInput& in, Motion& out) {
    std::uint8_t current[kThumbW * kThumbH];
    thumbnail(in, current);
    if (state->hasPrevious) {
      std::uint32_t diff = 0;
      for (int i = 0; i < kThumbW * kThumbH; ++i) {
        diff += static_cast<std::uint32_t>(std::abs(current[i] - state->previous[i]));
      }
      out.energy = static_cast<float>(diff) / (255.f * kThumbW * kThumbH);
    }
    std::memcpy(state->previous, current, sizeof(current));
    state->hasPrevious = true;
  });
  if (fpe::Status s = d->engine->start(); s != fpe::Status::Ok) {
    std::string msg = std::string("engine: ") + fpe::toString(s) + " " + d->engine->errorMessage();
    shutdown(d);
    return env->NewStringUTF(msg.c_str());
  }
  d->source = std::make_unique<fpe::android::ImageReaderSource>(
      *d->store, *d->engine,
      fpe::android::ImageReaderConfig{.width = kWidth, .height = kHeight,
                                      .maxFramesInFlight = kFramesInFlight});
  if (d->source->open() != fpe::Status::Ok || d->source->start() != fpe::Status::Ok) {
    shutdown(d);
    return env->NewStringUTF("cannot create the image reader");
  }
  if (const char* err = openCamera(*d, d->source->window())) {
    shutdown(d);
    return env->NewStringUTF(err);
  }
  d->lastStatsNs = nowNs(CLOCK_MONOTONIC);
  gDemo = d;
  LOG("started: %dx%d YUV, %u frames in flight", kWidth, kHeight, kFramesInFlight);
  return nullptr;
}

extern "C" JNIEXPORT void JNICALL Java_com_vtrsoftware_fpe_camerademo_NativeDemo_stop(JNIEnv*, jobject) {
  shutdown(gDemo);
  gDemo = nullptr;
}

extern "C" JNIEXPORT jstring JNICALL Java_com_vtrsoftware_fpe_camerademo_NativeDemo_stats(JNIEnv* env, jobject) {
  Demo* d = gDemo;
  if (d == nullptr) {
    return env->NewStringUTF("stopped");
  }
  const std::int64_t now = nowNs(CLOCK_MONOTONIC);
  const std::uint64_t results = d->results.load(std::memory_order_relaxed);
  const double seconds = static_cast<double>(now - d->lastStatsNs) / 1e9;
  if (seconds > 0.2) {
    d->fps = static_cast<float>(static_cast<double>(results - d->lastResults) / seconds);
    d->lastResults = results;
    d->lastStatsNs = now;
  }
  const fpe::StreamStats es = d->engine->stats(0);
  const fpe::android::ImageReaderStats rs = d->source->stats();
  char text[640];
  std::snprintf(text, sizeof(text),
                "FrameProcessorEngine\n"
                "camera     %dx%d YUV_420_888\n"
                "processed  %.1f fps\n"
                "latency    %.1f ms (sensor → result)\n"
                "results    %llu\n"
                "dropped    %llu\n"
                "  camera   %llu  (reader had no slot)\n"
                "  store    %llu  (no free frame slot)\n"
                "  input    %llu  (newer frame arrived)\n"
                "  no job   %llu\n"
                "luma       %.1f\n"
                "motion     %.3f",
                kWidth, kHeight, d->fps, d->latencyMs.load(), static_cast<unsigned long long>(results),
                static_cast<unsigned long long>(rs.acquireFailures + rs.wrapFailures +
                                                es.replacedInput + es.droppedNoJob),
                static_cast<unsigned long long>(rs.acquireFailures),
                static_cast<unsigned long long>(rs.wrapFailures),
                static_cast<unsigned long long>(es.replacedInput),
                static_cast<unsigned long long>(es.droppedNoJob), d->luma.load(), d->motionEnergy.load());
  return env->NewStringUTF(text);
}
