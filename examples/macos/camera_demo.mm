// FrameProcessorEngine camera demo for macOS (F7): AVCaptureSession on the default camera,
// 640x480 bi-planar 4:2:0, into fpe::apple::SampleBufferSource; two tasks per frame (mean luma,
// motion energy), as the Android example; prints fps, latency and drops once a second.
//
//   camera_demo [seconds=10]
//
// The terminal (or app) running it needs the camera permission (System Settings → Privacy).
#import <AVFoundation/AVFoundation.h>

#include <fpe/apple/camera.hpp>
#include <fpe/engine.hpp>

#include <mach/mach_time.h>

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int kThumbW = 32, kThumbH = 24;
struct Luma { float mean = 0; };
struct Motion { float energy = 0; };
struct MotionState { std::uint8_t previous[kThumbW * kThumbH]{}; bool has = false; };

std::int64_t hostNs() {
  static mach_timebase_info_data_t tb = [] { mach_timebase_info_data_t t; mach_timebase_info(&t); return t; }();
  return static_cast<std::int64_t>(mach_absolute_time() * tb.numer / tb.denom);
}

struct Stats {
  std::atomic<std::uint64_t> results{0};
  std::atomic<float> latencyMs{0}, luma{0}, motion{0};
  fpe::TaskId lumaTask = fpe::kInvalidTask, motionTask = fpe::kInvalidTask;
};

void onResult(fpe::FrameResult&& r, void* ctx) noexcept {
  auto* s = static_cast<Stats*>(ctx);
  s->latencyMs.store(static_cast<float>(hostNs() - r.info().timestampNs) / 1e6f, std::memory_order_relaxed);
  if (const Luma* l = r.get<Luma>(s->lumaTask)) s->luma.store(l->mean, std::memory_order_relaxed);
  if (const Motion* m = r.get<Motion>(s->motionTask)) s->motion.store(m->energy, std::memory_order_relaxed);
  s->results.fetch_add(1, std::memory_order_relaxed);
}

} // namespace

@interface Delegate : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
@property(nonatomic) fpe::apple::SampleBufferSource* source;
@end
@implementation Delegate
- (void)captureOutput:(AVCaptureOutput*)o didOutputSampleBuffer:(CMSampleBufferRef)sb fromConnection:(AVCaptureConnection*)c {
  self.source->submit(sb);
}
@end

int main(int argc, char** argv) {
  @autoreleasepool {
    const int seconds = argc > 1 ? std::atoi(argv[1]) : 10;
    __block bool granted = false;
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:^(BOOL ok) {
      granted = ok;
      dispatch_semaphore_signal(sem);
    }];
    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
    if (!granted) {
      std::fprintf(stderr, "no camera permission\n");
      return 1;
    }
    AVCaptureDevice* dev = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
    if (dev == nil) {
      std::fprintf(stderr, "no camera\n");
      return 1;
    }

    fpe::FrameStore store({.poolSlots = 0, .externalSlots = 6});
    Stats stats;
    fpe::Engine engine(store, {.workers = 2, .streams = 1, .maxFramesInFlight = 3, .onResult = {&onResult, &stats}});
    stats.lumaTask = engine.addTask<Luma>("luma", [](const fpe::TaskInput& in, Luma& out) {
      const fpe::Planes& p = in.planes();
      std::uint64_t sum = 0, n = 0;
      for (std::uint32_t y = 0; y < in.info().height; y += 4)
        for (std::uint32_t x = 0; x < in.info().width; x += 4, ++n) sum += p.data[0][y * p.stride[0] + x];
      out.mean = n ? static_cast<float>(sum) / static_cast<float>(n) : 0.f;
    });
    MotionState state;
    MotionState* st = &state;
    stats.motionTask = engine.addTask<Motion>("motion", [st](const fpe::TaskInput& in, Motion& out) {
      const fpe::Planes& p = in.planes();
      std::uint8_t cur[kThumbW * kThumbH];
      for (int ty = 0; ty < kThumbH; ++ty)
        for (int tx = 0; tx < kThumbW; ++tx)
          cur[ty * kThumbW + tx] = p.data[0][(static_cast<std::uint32_t>(ty) * in.info().height / kThumbH) * p.stride[0] +
                                             static_cast<std::uint32_t>(tx) * in.info().width / kThumbW];
      if (st->has) {
        std::uint32_t d = 0;
        for (int i = 0; i < kThumbW * kThumbH; ++i) d += static_cast<std::uint32_t>(std::abs(cur[i] - st->previous[i]));
        out.energy = static_cast<float>(d) / (255.f * kThumbW * kThumbH);
      }
      std::memcpy(st->previous, cur, sizeof(cur));
      st->has = true;
    });
    if (engine.start() != fpe::Status::Ok) {
      std::fprintf(stderr, "engine: %s\n", engine.errorMessage().c_str());
      return 1;
    }
    fpe::apple::SampleBufferSource source(store, engine);

    AVCaptureSession* session = [[AVCaptureSession alloc] init];
    session.sessionPreset = AVCaptureSessionPreset640x480;
    NSError* err = nil;
    AVCaptureDeviceInput* input = [AVCaptureDeviceInput deviceInputWithDevice:dev error:&err];
    if (input == nil || ![session canAddInput:input]) {
      std::fprintf(stderr, "cannot open the camera\n");
      return 1;
    }
    [session addInput:input];
    AVCaptureVideoDataOutput* out = [[AVCaptureVideoDataOutput alloc] init];
    out.videoSettings = @{(id)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_420YpCbCr8BiPlanarFullRange)};
    out.alwaysDiscardsLateVideoFrames = YES;
    Delegate* delegate = [[Delegate alloc] init];
    delegate.source = &source;
    [out setSampleBufferDelegate:delegate queue:dispatch_queue_create("camera", DISPATCH_QUEUE_SERIAL)];
    [session addOutput:out];
    [session startRunning];

    std::uint64_t last = 0;
    for (int s = 0; s < seconds; ++s) {
      [NSThread sleepForTimeInterval:1.0];
      const std::uint64_t r = stats.results.load();
      const fpe::StreamStats es = engine.stats(0);
      const fpe::apple::SampleBufferSourceStats ss = source.stats();
      std::printf("%2ds: %3llu fps, latency %5.1f ms, luma %5.1f, motion %.3f, dropped %llu (engine %llu, wrap %llu)\n", s + 1,
                  static_cast<unsigned long long>(r - last), static_cast<double>(stats.latencyMs.load()),
                  static_cast<double>(stats.luma.load()), static_cast<double>(stats.motion.load()),
                  static_cast<unsigned long long>(es.replacedInput + es.droppedNoJob + ss.wrapFailures),
                  static_cast<unsigned long long>(es.replacedInput + es.droppedNoJob),
                  static_cast<unsigned long long>(ss.wrapFailures));
      last = r;
    }
    [session stopRunning];
    engine.stop(); // releases every frame (and its pixel buffer)
  }
  return 0;
}
