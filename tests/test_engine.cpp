#include "support/alloc_guard.h"

#include <fpe/engine.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

using namespace fpe;
using namespace std::chrono_literals;

namespace {

constexpr std::uint32_t kW = 32;
constexpr std::uint32_t kH = 24;

std::int64_t nowNs() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

FrameStoreConfig storeConfig(std::uint32_t slots = 8) {
  return {.poolSlots = slots, .poolSlotBytes = kW * kH, .externalSlots = 0};
}

/// Fills a pool frame with a deterministic Gray8 pattern derived from `id`.
FrameRef makeFrame(FrameStore& store, std::uint32_t stream, std::uint64_t id) {
  FrameHandle h;
  if (store.acquire(h) != Status::Ok) {
    return {};
  }
  FrameInfo* info = store.mutableInfo(h);
  info->streamId = stream;
  info->frameId = id;
  info->timestampNs = nowNs();
  info->width = kW;
  info->height = kH;
  info->format = PixelFormat::Gray8;
  Planes* p = store.mutablePlanes(h);
  p->stride[0] = kW;
  std::uint8_t* px = store.poolMemory(h);
  for (std::uint32_t y = 0; y < kH; ++y) {
    for (std::uint32_t x = 0; x < kW; ++x) {
      px[y * kW + x] = static_cast<std::uint8_t>((x * 7 + y * 13 + id * 31) & 0xFF);
    }
  }
  return FrameRef(store, h);
}

struct Sum {
  std::uint64_t value = 0;
};
struct Edges {
  std::uint32_t count = 0;
};
struct Combined {
  std::uint64_t value = 0;
};

void sumPixels(const TaskInput& in, Sum& out) {
  const auto& p = in.planes();
  const auto& info = in.info();
  for (std::uint32_t i = 0; i < info.width * info.height; ++i) {
    out.value += p.data[0][i];
  }
}

void countEdges(const TaskInput& in, Edges& out) {
  const auto& p = in.planes();
  const auto& info = in.info();
  for (std::uint32_t y = 0; y < info.height; ++y) {
    for (std::uint32_t x = 1; x < info.width; ++x) {
      const int a = p.data[0][y * info.width + x];
      const int b = p.data[0][y * info.width + x - 1];
      if (a - b > 16 || b - a > 16) {
        ++out.count;
      }
    }
  }
}

struct Graph {
  TaskId sum, edges, combined;
};

Graph addThreeTasks(Engine& e) {
  Graph g{};
  g.sum = e.addTask<Sum>("sum", sumPixels);
  g.edges = e.addTask<Edges>("edges", countEdges);
  g.combined = e.addTask<Combined>(
      "combined",
      [s = g.sum, ed = g.edges](const TaskInput& in, Combined& out) {
        out.value = in.input<Sum>(s).value * 1000003u + in.input<Edges>(ed).count;
      },
      {g.sum, g.edges});
  return g;
}

struct Interval {
  std::int64_t start = 0;
  std::int64_t end = 0;
};

void armWorker(std::uint32_t, void*) noexcept { fpe::test::armCurrentThread(); }

} // namespace

// ---- registration ----------------------------------------------------------------------------

TEST(EngineRegistration, DuplicateNameRejected) {
  FrameStore store(storeConfig());
  Engine e(store, {});
  e.addTask<Sum>("a", sumPixels);
  EXPECT_EQ(e.addTask<Sum>("a", sumPixels), kInvalidTask);
  EXPECT_EQ(e.start(), Status::DuplicateTask);
  EXPECT_EQ(e.workerCount(), 0u);
}

TEST(EngineRegistration, UnknownDependencyRejected) {
  FrameStore store(storeConfig());
  Engine e(store, {});
  EXPECT_EQ(e.addTask<Sum>("a", sumPixels, {TaskId{7}}), kInvalidTask);
  EXPECT_EQ(e.start(), Status::UnknownDependency);
}

TEST(EngineRegistration, CycleRejectedAndNamed) {
  FrameStore store(storeConfig());
  Engine e(store, {});
  TaskId a = e.addTask<Sum>("alpha", sumPixels);
  TaskId b = e.addTask<Sum>("beta", sumPixels, {a});
  ASSERT_EQ(e.addDependency(a, b), Status::Ok);
  EXPECT_EQ(e.start(), Status::GraphCycle);
  EXPECT_NE(e.errorMessage().find("alpha"), std::string::npos);
  EXPECT_NE(e.errorMessage().find("beta"), std::string::npos);
  EXPECT_EQ(e.workerCount(), 0u);
  EXPECT_FALSE(e.running());
}

TEST(EngineRegistration, AddTaskAfterStartRejected) {
  FrameStore store(storeConfig());
  Engine e(store, {});
  e.addTask<Sum>("a", sumPixels);
  ASSERT_EQ(e.start(), Status::Ok);
  EXPECT_EQ(e.addTask<Sum>("b", sumPixels), kInvalidTask);
  e.stop();
}

// ---- scheduling ------------------------------------------------------------------------------

TEST(Engine, DiamondRunsIndependentTasksInParallel) {
  FrameStore store(storeConfig());
  Engine e(store, {.workers = 3, .mode = Mode::Offline});
  auto timed = [](const TaskInput&, Interval& out) {
    out.start = nowNs();
    std::this_thread::sleep_for(30ms);
    out.end = nowNs();
  };
  TaskId a = e.addTask<Interval>("A", timed);
  TaskId b = e.addTask<Interval>("B", timed, {a});
  TaskId c = e.addTask<Interval>("C", timed, {a});
  TaskId d = e.addTask<Interval>("D", timed, {b, c});
  ASSERT_EQ(e.start(), Status::Ok);

  FrameResult r;
  ASSERT_EQ(e.process(0, makeFrame(store, 0, 1), r), Status::Ok);
  const Interval& ia = *r.get<Interval>(a);
  const Interval& ib = *r.get<Interval>(b);
  const Interval& ic = *r.get<Interval>(c);
  const Interval& id = *r.get<Interval>(d);
  EXPECT_GE(ib.start, ia.end);
  EXPECT_GE(ic.start, ia.end);
  EXPECT_LT(ib.start, ic.end); // B and C overlap
  EXPECT_LT(ic.start, ib.end);
  EXPECT_GE(id.start, std::max(ib.end, ic.end));
  r.reset();
  e.stop();
  EXPECT_EQ(store.inUse(), 0u);
}

TEST(Engine, FailedTaskSkipsItsDependents) {
  FrameStore store(storeConfig());
  Engine e(store, {.workers = 2, .mode = Mode::Offline});
  TaskId a = e.addTask<Sum>("A", sumPixels);
  TaskId b = e.addTask<Sum>("B", [](const TaskInput&, Sum&) { return false; }, {a});
  TaskId c = e.addTask<Edges>("C", countEdges, {a});
  TaskId d = e.addTask<Sum>("D", sumPixels, {b, c});
  ASSERT_EQ(e.start(), Status::Ok);
  FrameResult r;
  ASSERT_EQ(e.process(0, makeFrame(store, 0, 3), r), Status::Ok);
  EXPECT_EQ(r.status(a), TaskStatus::Ok);
  EXPECT_EQ(r.status(b), TaskStatus::Failed);
  EXPECT_EQ(r.status(c), TaskStatus::Ok);
  EXPECT_EQ(r.status(d), TaskStatus::Skipped);
  EXPECT_EQ(r.get<Sum>(b), nullptr);
  EXPECT_NE(r.get<Edges>(c), nullptr);
  EXPECT_EQ(r.get<Sum>(c), nullptr); // wrong type
  EXPECT_EQ(r.info().frameId, 3u);
}

// ---- offline ---------------------------------------------------------------------------------

namespace {
std::vector<std::uint64_t> runOffline(std::uint32_t workers, int frames, std::uint64_t* dropped) {
  FrameStore store(storeConfig(4));
  Engine e(store, {.workers = workers, .maxFramesInFlight = 2, .mode = Mode::Offline});
  Graph g = addThreeTasks(e);
  EXPECT_EQ(e.start(), Status::Ok);
  std::vector<std::uint64_t> out;
  out.reserve(static_cast<std::size_t>(frames));
  for (int i = 0; i < frames; ++i) {
    FrameResult r;
    EXPECT_EQ(e.process(0, makeFrame(store, 0, static_cast<std::uint64_t>(i)), r), Status::Ok);
    EXPECT_EQ(r.info().frameId, static_cast<std::uint64_t>(i));
    out.push_back(r.get<Combined>(g.combined)->value);
  }
  StreamStats s = e.stats(0);
  *dropped = s.replacedInput + s.droppedNoJob + s.replacedResults;
  EXPECT_EQ(s.completed, static_cast<std::uint64_t>(frames));
  e.stop();
  EXPECT_EQ(store.inUse(), 0u);
  return out;
}
} // namespace

TEST(EngineOffline, DeterministicAcrossRunsAndWorkerCounts) {
  std::uint64_t dropped = 0;
  auto a = runOffline(1, 1000, &dropped);
  EXPECT_EQ(dropped, 0u);
  auto b = runOffline(1, 1000, &dropped);
  EXPECT_EQ(dropped, 0u);
  auto c = runOffline(8, 1000, &dropped);
  EXPECT_EQ(dropped, 0u);
  auto d = runOffline(8, 1000, &dropped);
  EXPECT_EQ(dropped, 0u);
  EXPECT_EQ(a, b);
  EXPECT_EQ(a, c);
  EXPECT_EQ(a, d);
}

TEST(EngineOffline, NoAllocationsOnAnyEngineThread) {
  FrameStore store(storeConfig(4));
  Engine e(store, {.workers = 3, .maxFramesInFlight = 2, .mode = Mode::Offline,
                   .onWorkerStart = {&armWorker, nullptr}});
  Graph g = addThreeTasks(e);
  ASSERT_EQ(e.start(), Status::Ok);
  std::uint64_t checksum = 0;
  // Warm up once (first-use paths in the standard library), then measure.
  {
    FrameResult r;
    ASSERT_EQ(e.process(0, makeFrame(store, 0, 0), r), Status::Ok);
  }
  fpe::test::resetAllocationCount();
  EXPECT_NO_ALLOCATIONS({
    for (std::uint64_t i = 1; i <= 1000; ++i) {
      FrameResult r;
      e.process(0, makeFrame(store, 0, i), r);
      checksum += r.get<Combined>(g.combined)->value;
    }
  });
  EXPECT_EQ(fpe::test::allocationCount(), 0);
  e.stop();
  EXPECT_NE(checksum, 0u);
}

// ---- real time -------------------------------------------------------------------------------

TEST(EngineRealTime, SlowConsumerGetsNewestResults) {
  FrameStore store(storeConfig(8));
  Engine e(store, {.workers = 2, .streams = 1, .maxFramesInFlight = 3});
  Graph g = addThreeTasks(e);
  ASSERT_EQ(e.start(), Status::Ok);

  std::atomic<bool> done{false};
  std::thread producer([&] {
    auto next = std::chrono::steady_clock::now();
    for (std::uint64_t i = 1; i <= 120; ++i) { // 2 s at 60 fps
      if (FrameRef f = makeFrame(store, 0, i)) {
        e.submit(0, std::move(f));
      }
      next += 16667us;
      std::this_thread::sleep_until(next);
    }
    done.store(true, std::memory_order_release);
  });

  std::uint64_t lastId = 0;
  std::uint64_t received = 0;
  bool increasing = true;
  bool timestampsOk = true;
  while (!done.load(std::memory_order_acquire)) {
    if (FrameResult r = e.takeResult(0)) {
      if (r.info().frameId <= lastId) {
        increasing = false;
      }
      if (r.info().timestampNs <= 0 || r.info().timestampNs > nowNs()) {
        timestampsOk = false;
      }
      lastId = r.info().frameId;
      EXPECT_EQ(r.status(g.combined), TaskStatus::Ok);
      ++received;
    }
    std::this_thread::sleep_for(45ms); // slower than the producer
  }
  producer.join();
  std::this_thread::sleep_for(50ms);
  if (FrameResult r = e.takeResult(0)) {
    EXPECT_GT(r.info().frameId, lastId);
    lastId = r.info().frameId;
    ++received;
  }

  EXPECT_TRUE(increasing);
  EXPECT_TRUE(timestampsOk);
  EXPECT_GT(received, 10u);
  EXPECT_EQ(lastId, 120u); // the newest frame is never lost
  StreamStats s = e.stats(0);
  EXPECT_EQ(s.submitted, 120u);
  EXPECT_GT(s.replacedResults, 0u);
  EXPECT_EQ(s.completed, s.replacedResults + received);
  e.stop();
  EXPECT_EQ(store.inUse(), 0u);
}

TEST(EngineRealTime, TwoStreamsAndCallbackDelivery) {
  struct Sink {
    std::atomic<std::uint64_t> perStream[2]{};
    TaskId combined = kInvalidTask;
    std::atomic<bool> allOk{true};
    static void onResult(FrameResult&& r, void* ctx) noexcept {
      auto* self = static_cast<Sink*>(ctx);
      if (r.status(self->combined) != TaskStatus::Ok || r.info().streamId != r.stream()) {
        self->allOk.store(false);
      }
      self->perStream[r.stream()].fetch_add(1);
    }
  } sink;
  FrameStore store(storeConfig(8));
  Engine e(store, {.workers = 2, .streams = 2, .maxFramesInFlight = 4,
                   .onResult = {&Sink::onResult, &sink}});
  sink.combined = addThreeTasks(e).combined;
  ASSERT_EQ(e.start(), Status::Ok);
  std::vector<std::thread> producers;
  for (std::uint32_t s = 0; s < 2; ++s) {
    producers.emplace_back([&, s] {
      for (std::uint64_t i = 1; i <= 200; ++i) {
        if (FrameRef f = makeFrame(store, s, i)) {
          e.submit(s, std::move(f));
        }
        std::this_thread::sleep_for(1ms);
      }
    });
  }
  for (auto& p : producers) {
    p.join();
  }
  e.stop();
  EXPECT_TRUE(sink.allOk.load());
  EXPECT_GT(sink.perStream[0].load(), 0u);
  EXPECT_GT(sink.perStream[1].load(), 0u);
  EXPECT_EQ(store.inUse(), 0u);
}

TEST(EngineRealTime, StopWithWorkPendingReleasesEverything) {
  FrameStore store(storeConfig(8));
  Engine e(store, {.workers = 1, .streams = 1, .maxFramesInFlight = 2});
  e.addTask<Sum>("slow", [](const TaskInput& in, Sum& out) {
    std::this_thread::sleep_for(20ms);
    out.value = in.info().frameId;
  });
  ASSERT_EQ(e.start(), Status::Ok);
  for (std::uint64_t i = 1; i <= 5; ++i) {
    e.submit(0, makeFrame(store, 0, i));
  }
  e.stop(); // the in-flight frame finishes; the pending one and the result are released
  EXPECT_FALSE(e.running());
  EXPECT_EQ(store.inUse(), 0u);
  EXPECT_EQ(e.submit(0, makeFrame(store, 0, 9)), Status::NotStarted);
  EXPECT_EQ(store.inUse(), 0u); // the rejected frame was released by its FrameRef
}

TEST(EngineRealTime, NoAllocationsOnAnyEngineThread) {
  FrameStore store(storeConfig(8));
  Engine e(store, {.workers = 2, .streams = 1, .maxFramesInFlight = 3,
                   .onWorkerStart = {&armWorker, nullptr}});
  Graph g = addThreeTasks(e);
  ASSERT_EQ(e.start(), Status::Ok);
  // Warm-up frame.
  e.submit(0, makeFrame(store, 0, 0));
  std::this_thread::sleep_for(20ms);
  e.takeResult(0).reset();
  fpe::test::resetAllocationCount();
  std::uint64_t got = 0;
  EXPECT_NO_ALLOCATIONS({
    for (std::uint64_t i = 1; i <= 1000; ++i) {
      if (FrameRef f = makeFrame(store, 0, i)) {
        e.submit(0, std::move(f));
      }
      if (FrameResult r = e.takeResult(0)) {
        got += r.get<Combined>(g.combined) != nullptr ? 1u : 0u;
      }
      std::this_thread::sleep_for(100us);
    }
  });
  EXPECT_EQ(fpe::test::allocationCount(), 0);
  e.stop();
  EXPECT_GT(got, 0u);
}
