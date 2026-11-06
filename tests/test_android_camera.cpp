// Host tests of platform/android against the fake NDK in tests/android_host.

#include "android_host/fake_ndk.h"
#include "support/alloc_guard.h"

#include <fpe/android/camera.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <thread>
#include <vector>

using namespace fpe;
using namespace std::chrono_literals;

namespace {

struct Yuv {
  AImage image;
  std::uint8_t memory[64 * 48 * 2]{};
  explicit Yuv(std::int64_t ts = 123'456'789) { fakendk::makeYuv(image, memory, 64, 48, ts); }
};

} // namespace

TEST(AndroidCamera, YuvImageIsWrappedWithoutCopy) {
  FrameStore store({.poolSlots = 0, .externalSlots = 2});
  Yuv yuv;
  FrameRef f;
  ASSERT_EQ(android::wrapAImage(store, &yuv.image, 3, 42, f), Status::Ok);
  const FrameInfo& info = *f.info();
  EXPECT_EQ(info.format, PixelFormat::YUV420);
  EXPECT_EQ(info.width, 64u);
  EXPECT_EQ(info.height, 48u);
  EXPECT_EQ(info.timestampNs, 123'456'789);
  EXPECT_EQ(info.streamId, 3u);
  EXPECT_EQ(info.frameId, 42u);
  const Planes& p = *f.planes();
  ASSERT_EQ(p.count, 3u);
  EXPECT_EQ(p.data[0], yuv.image.data[0]); // same memory: no copy
  EXPECT_EQ(p.data[1], yuv.image.data[1]);
  EXPECT_EQ(p.data[2], yuv.image.data[2]);
  EXPECT_EQ(p.stride[0], 64u);
  EXPECT_EQ(p.pixelStride[0], 1u);
  EXPECT_EQ(p.pixelStride[1], 2u); // interleaved chroma
  EXPECT_EQ(p.pixelStride[2], 2u);
  EXPECT_EQ(p.native, nullptr);
  f.reset();
  EXPECT_EQ(yuv.image.deletes.load(), 1);
}

TEST(AndroidCamera, SharedOwnersDeleteTheImageOnce) {
  FrameStore store({.poolSlots = 0, .externalSlots = 1});
  Yuv yuv;
  FrameRef a;
  ASSERT_EQ(android::wrapAImage(store, &yuv.image, 0, 1, a), Status::Ok);
  FrameRef b = a.share();
  FrameRef c = a.share();
  a.reset();
  b.reset();
  EXPECT_EQ(yuv.image.deletes.load(), 0);
  c.reset();
  EXPECT_EQ(yuv.image.deletes.load(), 1);
}

TEST(AndroidCamera, ExhaustedStoreLeavesTheImageWithTheCaller) {
  FrameStore store({.poolSlots = 0, .externalSlots = 1});
  Yuv first, second;
  FrameRef a, b;
  ASSERT_EQ(android::wrapAImage(store, &first.image, 0, 1, a), Status::Ok);
  EXPECT_EQ(android::wrapAImage(store, &second.image, 0, 2, b), Status::PoolExhausted);
  EXPECT_FALSE(b);
  EXPECT_EQ(second.image.deletes.load(), 0);
}

TEST(AndroidCamera, PrivateImageExposesItsHardwareBuffer) {
  FrameStore store({.poolSlots = 0, .externalSlots = 1});
  AHardwareBuffer buffer;
  AImage image;
  image.format = AIMAGE_FORMAT_PRIVATE;
  image.width = 1920;
  image.height = 1080;
  image.buffer = &buffer;
  FrameRef f;
  ASSERT_EQ(android::wrapAImage(store, &image, 0, 1, f), Status::Ok);
  EXPECT_EQ(f.info()->format, PixelFormat::Opaque);
  EXPECT_EQ(f.planes()->count, 0u);
  EXPECT_EQ(f.planes()->native, &buffer);
}

TEST(AndroidCamera, UnsupportedFormatIsRejected) {
  FrameStore store({.poolSlots = 0, .externalSlots = 1});
  AImage image;
  image.format = AIMAGE_FORMAT_JPEG;
  FrameRef f;
  EXPECT_EQ(android::wrapAImage(store, &image, 0, 1, f), Status::InvalidArgument);
  EXPECT_EQ(image.deletes.load(), 0);
  EXPECT_EQ(store.inUse(), 0u);
}

TEST(AndroidCamera, HardwareBufferReferencesAreBalanced) {
  FrameStore store({.poolSlots = 0, .externalSlots = 1});
  AHardwareBuffer buffer;
  FrameInfo info;
  info.width = 640;
  info.height = 480;
  FrameRef f;
  ASSERT_EQ(android::wrapHardwareBuffer(store, &buffer, info, f), Status::Ok);
  EXPECT_EQ(f.planes()->native, &buffer);
  EXPECT_EQ(buffer.acquires.load(), 1);
  EXPECT_EQ(buffer.releases.load(), 0);
  f.reset();
  EXPECT_EQ(buffer.acquires.load(), 1);
  EXPECT_EQ(buffer.releases.load(), 1);
}

namespace {

struct Luma {
  std::uint64_t sum = 0;
};

void sumLuma(const TaskInput& in, Luma& out) {
  const Planes& p = in.planes();
  for (std::uint32_t y = 0; y < in.info().height; ++y) {
    for (std::uint32_t x = 0; x < in.info().width; ++x) {
      out.sum += p.data[0][y * p.stride[0] + x * p.pixelStride[0]];
    }
  }
}

} // namespace

TEST(AndroidCamera, ImageReaderSourceFeedsTheEngine) {
  fakendk::resetReader();
  FrameStore store({.poolSlots = 0, .externalSlots = 8});
  Engine engine(store, {.workers = 2, .streams = 1, .maxFramesInFlight = 3});
  TaskId luma = engine.addTask<Luma>("luma", sumLuma);
  ASSERT_EQ(engine.start(), Status::Ok);
  {
    android::ImageReaderSource source(store, engine,
                                      {.width = 64, .height = 48, .stream = 0, .maxFramesInFlight = 3});
    ASSERT_EQ(source.open(), Status::Ok);
    EXPECT_NE(source.window(), nullptr);
    EXPECT_EQ(fakendk::reader().maxImages, 5); // maxFramesInFlight + 2
    ASSERT_EQ(source.start(), Status::Ok);

    std::int64_t lastTs = 0;
    int results = 0;
    for (int i = 0; i < 300; ++i) {
      fakendk::produce(); // the camera delivers a frame
      if (FrameResult r = engine.takeResult(0)) {
        EXPECT_GT(r.info().timestampNs, lastTs);
        lastTs = r.info().timestampNs;
        EXPECT_EQ(r.info().format, PixelFormat::YUV420);
        EXPECT_NE(r.get<Luma>(luma), nullptr);
        ++results;
      }
      std::this_thread::sleep_for(1ms);
    }
    EXPECT_GT(results, 50);
    const auto st = source.stats();
    EXPECT_GT(st.images, 100u);
    EXPECT_EQ(st.rejected, 0u);

    source.stop();
    engine.stop(); // releases every frame still held
    EXPECT_EQ(source.outstanding(), 0u);
    EXPECT_EQ(fakendk::reader().held.load(), 0);
  }
  EXPECT_TRUE(fakendk::reader().deleted);
  EXPECT_EQ(store.inUse(), 0u);
}

TEST(AndroidCamera, ImageCallbackDoesNotAllocate) {
  fakendk::resetReader();
  FrameStore store({.poolSlots = 0, .externalSlots = 8});
  Engine engine(store, {.workers = 1, .streams = 1, .maxFramesInFlight = 3});
  engine.addTask<Luma>("luma", sumLuma);
  ASSERT_EQ(engine.start(), Status::Ok);
  android::ImageReaderSource source(store, engine,
                                    {.width = 64, .height = 48, .stream = 0, .maxFramesInFlight = 3});
  ASSERT_EQ(source.open(), Status::Ok);
  ASSERT_EQ(source.start(), Status::Ok);
  fakendk::produce(); // warm-up
  std::this_thread::sleep_for(5ms);
  engine.takeResult(0).reset();
  EXPECT_NO_ALLOCATIONS({
    for (int i = 0; i < 200; ++i) {
      fakendk::produce(); // runs ImageReaderSource's callback on this thread
      engine.takeResult(0).reset();
    }
  });
  source.stop();
  engine.stop();
  EXPECT_EQ(source.outstanding(), 0u);
}

TEST(AndroidCamera, StoppedEngineRejectsAndReleasesImages) {
  fakendk::resetReader();
  FrameStore store({.poolSlots = 0, .externalSlots = 4});
  Engine engine(store, {.workers = 1, .streams = 1, .maxFramesInFlight = 2});
  android::ImageReaderSource source(store, engine,
                                    {.width = 64, .height = 48, .stream = 0, .maxFramesInFlight = 2});
  ASSERT_EQ(source.open(), Status::Ok);
  ASSERT_EQ(source.start(), Status::Ok);
  fakendk::produce(); // engine never started
  EXPECT_EQ(source.stats().rejected, 1u);
  EXPECT_EQ(source.outstanding(), 0u);
  EXPECT_EQ(fakendk::reader().held.load(), 0);
}
