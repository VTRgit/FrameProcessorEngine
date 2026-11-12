// F7: the Apple adapter on real CoreVideo/CoreMedia objects (macOS host).
#include "support/alloc_guard.h"

#include <fpe/apple/camera.hpp>
#include <fpe/engine.hpp>
#include <fpe/frame.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

using namespace fpe;

namespace {

constexpr std::size_t kW = 64;
constexpr std::size_t kH = 48;

std::atomic<int> gFreed{0};

/// Planar memory owned by the test; the buffer's release callback counts frees.
struct Nv12Memory {
  std::vector<std::uint8_t> y = std::vector<std::uint8_t>(kW * kH, 80);
  std::vector<std::uint8_t> uv = std::vector<std::uint8_t>(kW * kH / 2, 128);
};

void countFree(void*, const void*, size_t, size_t, const void**) { gFreed.fetch_add(1); }
void countFreeBgra(void*, const void*) { gFreed.fetch_add(1); }

CVPixelBufferRef makeNv12(Nv12Memory& m) {
  void* bases[2] = {m.y.data(), m.uv.data()};
  size_t widths[2] = {kW, kW / 2};
  size_t heights[2] = {kH, kH / 2};
  size_t rows[2] = {kW, kW};
  CVPixelBufferRef b = nullptr;
  const CVReturn r = CVPixelBufferCreateWithPlanarBytes(kCFAllocatorDefault, kW, kH,
                                                        kCVPixelFormatType_420YpCbCr8BiPlanarFullRange, nullptr, 0,
                                                        2, bases, widths, heights, rows, &countFree, nullptr,
                                                        nullptr, &b);
  EXPECT_EQ(r, kCVReturnSuccess);
  return b;
}

CVPixelBufferRef makeBgra(std::vector<std::uint8_t>& m) {
  CVPixelBufferRef b = nullptr;
  EXPECT_EQ(CVPixelBufferCreateWithBytes(kCFAllocatorDefault, kW, kH, kCVPixelFormatType_32BGRA, m.data(), kW * 4,
                                         &countFreeBgra, nullptr, nullptr, &b),
            kCVReturnSuccess);
  return b;
}

CMSampleBufferRef makeSample(CVPixelBufferRef px, std::int64_t ns) {
  CMVideoFormatDescriptionRef desc = nullptr;
  EXPECT_EQ(CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, px, &desc), noErr);
  CMSampleTimingInfo timing{kCMTimeInvalid, CMTimeMake(ns, 1'000'000'000), kCMTimeInvalid};
  CMSampleBufferRef s = nullptr;
  EXPECT_EQ(CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault, px, desc, &timing, &s), noErr);
  CFRelease(desc);
  return s;
}

} // namespace

TEST(AppleCamera, Nv12BufferIsWrappedWithoutCopy) {
  Nv12Memory m;
  CVPixelBufferRef px = makeNv12(m);
  FrameStore store({.poolSlots = 0, .externalSlots = 2});
  {
    FrameRef f;
    ASSERT_EQ(apple::wrapPixelBuffer(store, px, 3, 7, 123'456, f), Status::Ok);
    const FrameInfo* info = store.info(f.handle());
    const Planes* p = store.planes(f.handle());
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->format, PixelFormat::NV12);
    EXPECT_EQ(info->width, kW);
    EXPECT_EQ(info->height, kH);
    EXPECT_EQ(info->timestampNs, 123'456);
    EXPECT_EQ(info->streamId, 3u);
    EXPECT_EQ(info->frameId, 7u);
    EXPECT_EQ(p->count, 2u);
    EXPECT_EQ(p->data[0], m.y.data()); // the buffer's own memory: no copy
    EXPECT_EQ(p->data[1], m.uv.data());
    EXPECT_EQ(p->stride[0], kW);
    EXPECT_EQ(p->stride[1], kW);
    EXPECT_EQ(p->pixelStride[0], 1u);
    EXPECT_EQ(p->pixelStride[1], 2u);
    EXPECT_EQ(p->native, px);
  }
  const int before = gFreed.load();
  CVPixelBufferRelease(px); // our reference; the frame's went with it
  EXPECT_EQ(gFreed.load(), before + 1);
}

TEST(AppleCamera, BgraBufferHasOnePlane) {
  std::vector<std::uint8_t> m(kW * kH * 4, 0);
  CVPixelBufferRef px = makeBgra(m);
  FrameStore store({.poolSlots = 0, .externalSlots = 1});
  {
    FrameRef f;
    ASSERT_EQ(apple::wrapPixelBuffer(store, px, 0, 1, 1, f), Status::Ok);
    const Planes* p = store.planes(f.handle());
    EXPECT_EQ(store.info(f.handle())->format, PixelFormat::BGRA8);
    EXPECT_EQ(p->count, 1u);
    EXPECT_EQ(p->data[0], m.data());
    EXPECT_EQ(p->stride[0], kW * 4);
  }
  CVPixelBufferRelease(px);
}

TEST(AppleCamera, OtherFormatsAreOpaque) {
  CVPixelBufferRef px = nullptr;
  ASSERT_EQ(CVPixelBufferCreate(kCFAllocatorDefault, kW, kH, kCVPixelFormatType_64RGBAHalf, nullptr, &px),
            kCVReturnSuccess);
  FrameStore store({.poolSlots = 0, .externalSlots = 1});
  {
    FrameRef f;
    ASSERT_EQ(apple::wrapPixelBuffer(store, px, 0, 1, 1, f), Status::Ok);
    EXPECT_EQ(store.info(f.handle())->format, PixelFormat::Opaque);
    EXPECT_EQ(store.planes(f.handle())->count, 0u);
    EXPECT_EQ(store.planes(f.handle())->native, px);
  }
  EXPECT_EQ(CFGetRetainCount(px), 1); // the frame's reference was released
  CVPixelBufferRelease(px);
}

TEST(AppleCamera, SharedOwnersReleaseOnceAfterTheLast) {
  Nv12Memory m;
  CVPixelBufferRef px = makeNv12(m);
  FrameStore store({.poolSlots = 0, .externalSlots = 1});
  FrameRef a;
  ASSERT_EQ(apple::wrapPixelBuffer(store, px, 0, 1, 1, a), Status::Ok);
  CVPixelBufferRelease(px); // only the frame holds it now
  const int before = gFreed.load();
  ASSERT_EQ(store.retain(a.handle()), Status::Ok);
  FrameRef b(store, a.handle());
  ASSERT_EQ(store.retain(a.handle()), Status::Ok);
  FrameRef c(store, a.handle());
  a.reset();
  b.reset();
  EXPECT_EQ(gFreed.load(), before); // still referenced by c
  c.reset();
  EXPECT_EQ(gFreed.load(), before + 1);
}

TEST(AppleCamera, StoreExhaustedLeavesTheBufferToTheCaller) {
  Nv12Memory m1, m2;
  CVPixelBufferRef p1 = makeNv12(m1);
  CVPixelBufferRef p2 = makeNv12(m2);
  FrameStore store({.poolSlots = 0, .externalSlots = 1});
  FrameRef held;
  ASSERT_EQ(apple::wrapPixelBuffer(store, p1, 0, 1, 1, held), Status::Ok);
  FrameRef f;
  EXPECT_EQ(apple::wrapPixelBuffer(store, p2, 0, 2, 2, f), Status::PoolExhausted);
  EXPECT_EQ(CFGetRetainCount(p2), 1);
  // Not left locked either: a write lock now succeeds.
  EXPECT_EQ(CVPixelBufferLockBaseAddress(p2, 0), kCVReturnSuccess);
  CVPixelBufferUnlockBaseAddress(p2, 0);
  held.reset();
  CVPixelBufferRelease(p1);
  CVPixelBufferRelease(p2);
}

TEST(AppleCamera, PresentationTimeInNanoseconds) {
  Nv12Memory m;
  CVPixelBufferRef px = makeNv12(m);
  CMSampleBufferRef s = makeSample(px, 9'876'543'210);
  EXPECT_EQ(apple::presentationTimeNs(s), 9'876'543'210);
  CFRelease(s);
  CVPixelBufferRelease(px);
}

namespace {
struct Luma {
  float mean = 0;
};
void meanLuma(const TaskInput& in, Luma& out) {
  const Planes& p = in.planes();
  std::uint64_t sum = 0;
  for (std::uint32_t y = 0; y < in.info().height; ++y)
    for (std::uint32_t x = 0; x < in.info().width; ++x) sum += p.data[0][y * p.stride[0] + x];
  out.mean = static_cast<float>(sum) / static_cast<float>(in.info().width * in.info().height);
}
} // namespace

TEST(AppleCamera, SampleBufferSourceFeedsTheEngine) {
  constexpr int kFrames = 20;
  std::vector<Nv12Memory> mem(kFrames);
  std::vector<CVPixelBufferRef> px(kFrames);
  std::vector<CMSampleBufferRef> samples(kFrames);
  const int freedBefore = gFreed.load();
  for (int i = 0; i < kFrames; ++i) {
    px[static_cast<std::size_t>(i)] = makeNv12(mem[static_cast<std::size_t>(i)]);
    samples[static_cast<std::size_t>(i)] = makeSample(px[static_cast<std::size_t>(i)], 1'000'000 * (i + 1));
    CVPixelBufferRelease(px[static_cast<std::size_t>(i)]); // the sample holds it
  }
  FrameStore store({.poolSlots = 0, .externalSlots = 5});
  Engine engine(store, {.workers = 2, .streams = 1, .maxFramesInFlight = 3});
  TaskId luma = engine.addTask<Luma>("luma", meanLuma);
  ASSERT_EQ(engine.start(), Status::Ok);
  apple::SampleBufferSource source(store, engine);
  std::vector<std::int64_t> seen;
  for (int i = 0; i < kFrames; ++i) {
    source.submit(samples[static_cast<std::size_t>(i)]);
    for (int spin = 0; spin < 200; ++spin) {
      if (FrameResult r = engine.takeResult(0)) {
        ASSERT_NE(r.get<Luma>(luma), nullptr);
        EXPECT_FLOAT_EQ(r.get<Luma>(luma)->mean, 80.0f);
        seen.push_back(r.info().timestampNs);
        break;
      }
      std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
  }
  engine.stop(); // releases every frame still held
  EXPECT_GE(seen.size(), 1u);
  for (std::size_t i = 1; i < seen.size(); ++i) EXPECT_GT(seen[i], seen[i - 1]); // in order
  EXPECT_EQ(source.stats().frames + source.stats().wrapFailures + source.stats().rejected,
            static_cast<std::uint64_t>(kFrames));
  for (CMSampleBufferRef s : samples) CFRelease(s);
  EXPECT_EQ(gFreed.load(), freedBefore + kFrames); // nothing stays retained
}

TEST(AppleCamera, SubmitDoesNotAllocate) {
  constexpr int kFrames = 8;
  std::vector<Nv12Memory> mem(kFrames);
  std::vector<CMSampleBufferRef> samples(kFrames);
  for (int i = 0; i < kFrames; ++i) {
    CVPixelBufferRef px = makeNv12(mem[static_cast<std::size_t>(i)]);
    samples[static_cast<std::size_t>(i)] = makeSample(px, 1'000 * (i + 1));
    CVPixelBufferRelease(px);
  }
  FrameStore store({.poolSlots = 0, .externalSlots = 5});
  Engine engine(store, {.workers = 1, .streams = 1, .maxFramesInFlight = 3});
  engine.addTask<Luma>("luma", meanLuma);
  ASSERT_EQ(engine.start(), Status::Ok);
  apple::SampleBufferSource source(store, engine);
  source.submit(samples[0]); // warm-up
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  engine.takeResult(0).reset();
  EXPECT_NO_ALLOCATIONS({
    for (int i = 1; i < kFrames; ++i) {
      source.submit(samples[static_cast<std::size_t>(i)]);
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
      engine.takeResult(0).reset();
    }
  });
  engine.stop();
  for (CMSampleBufferRef s : samples) CFRelease(s);
}
