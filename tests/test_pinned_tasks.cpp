#include "support/alloc_guard.h"

#include <fpe/engine.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <set>
#include <thread>

using namespace fpe;

namespace {

FrameRef frame(FrameStore& store, std::uint64_t id) {
  FrameHandle h;
  if (store.acquire(h) != Status::Ok) return {};
  store.mutableInfo(h)->frameId = id;
  return FrameRef(store, h);
}

struct Value {
  std::uint64_t v = 0;
};
struct Where {
  std::thread::id thread{};
  std::uint64_t v = 0;
};

struct Threads {
  std::mutex m;
  std::set<std::thread::id> workers;
  std::thread::id lane{};
  std::atomic<bool> laneHookRan{false};
  static void onWorker(std::uint32_t, void* c) noexcept {
    auto* t = static_cast<Threads*>(c);
    std::lock_guard<std::mutex> l(t->m);
    t->workers.insert(std::this_thread::get_id());
  }
  static void onLane(std::uint32_t lane, void* c) noexcept {
    auto* t = static_cast<Threads*>(c);
    if (lane == 0) {
      t->lane = std::this_thread::get_id();
      t->laneHookRan.store(true);
    }
  }
};

void armAll(std::uint32_t, void*) noexcept { fpe::test::armCurrentThread(); }

} // namespace

TEST(PinnedTasks, AlwaysTheSameLaneThreadAndHookRunsFirst) {
  Threads threads;
  FrameStore store({.poolSlots = 4, .poolSlotBytes = 8, .externalSlots = 0});
  Engine e(store, {.workers = 3, .maxFramesInFlight = 2, .mode = Mode::Offline,
                   .onWorkerStart = {&Threads::onWorker, &threads}, .lanes = 1,
                   .onLaneStart = {&Threads::onLane, &threads}});
  std::atomic<bool> hookBeforeTask{true};
  TaskId gpu = e.addTask<Where>(
      "gpu",
      [&](const TaskInput& in, Where& out) {
        if (!threads.laneHookRan.load()) hookBeforeTask.store(false);
        out.thread = std::this_thread::get_id();
        out.v = in.info().frameId * 2;
      },
      {}, {.lane = 0});
  ASSERT_EQ(e.start(), Status::Ok) << e.errorMessage();
  std::set<std::thread::id> seen;
  for (std::uint64_t i = 1; i <= 1000; ++i) {
    FrameResult r;
    ASSERT_EQ(e.process(0, frame(store, i), r), Status::Ok);
    seen.insert(r.get<Where>(gpu)->thread);
    EXPECT_EQ(r.get<Where>(gpu)->v, i * 2);
  }
  e.stop();
  ASSERT_EQ(seen.size(), 1u);
  EXPECT_EQ(*seen.begin(), threads.lane);
  EXPECT_EQ(threads.workers.count(threads.lane), 0u);
  EXPECT_TRUE(hookBeforeTask.load());
}

TEST(PinnedTasks, MixedGraphAcrossWorkersAndLanes) {
  FrameStore store({.poolSlots = 4, .poolSlotBytes = 8, .externalSlots = 0});
  Engine e(store, {.workers = 2, .maxFramesInFlight = 2, .mode = Mode::Offline, .lanes = 2});
  TaskId a = e.addTask<Value>("a", [](const TaskInput& in, Value& o) { o.v = in.info().frameId + 1; });
  TaskId b = e.addTask<Value>(
      "b", [a](const TaskInput& in, Value& o) { o.v = in.input<Value>(a).v * 10; }, {a}, {.lane = 0});
  TaskId c = e.addTask<Value>(
      "c", [a](const TaskInput& in, Value& o) { o.v = in.input<Value>(a).v * 100; }, {a}, {.lane = 1});
  TaskId d = e.addTask<Value>(
      "d", [b, c](const TaskInput& in, Value& o) { o.v = in.input<Value>(b).v + in.input<Value>(c).v; },
      {b, c});
  ASSERT_EQ(e.start(), Status::Ok);
  for (std::uint64_t i = 0; i < 500; ++i) {
    FrameResult r;
    ASSERT_EQ(e.process(0, frame(store, i), r), Status::Ok);
    EXPECT_EQ(r.get<Value>(d)->v, (i + 1) * 110);
  }
  e.stop();
  EXPECT_EQ(store.inUse(), 0u);
}

TEST(PinnedTasks, UnknownLaneRejected) {
  FrameStore store({.poolSlots = 1, .poolSlotBytes = 8, .externalSlots = 0});
  Engine e(store, {.lanes = 1});
  e.addTask<Value>("gpu-task", [](const TaskInput&, Value&) {}, {}, {.lane = 3});
  EXPECT_EQ(e.start(), Status::InvalidArgument);
  EXPECT_NE(e.errorMessage().find("gpu-task"), std::string::npos);
  EXPECT_FALSE(e.running());
}

TEST(PinnedTasks, RealTimeWithLanesNoAllocations) {
  FrameStore store({.poolSlots = 8, .poolSlotBytes = 8, .externalSlots = 0});
  Engine e(store, {.workers = 2, .maxFramesInFlight = 3, .onWorkerStart = {&armAll, nullptr},
                   .lanes = 1, .onLaneStart = {&armAll, nullptr}});
  TaskId a = e.addTask<Value>("a", [](const TaskInput& in, Value& o) { o.v = in.info().frameId; });
  TaskId b = e.addTask<Value>(
      "b", [a](const TaskInput& in, Value& o) { o.v = in.input<Value>(a).v + 1; }, {a}, {.lane = 0});
  ASSERT_EQ(e.start(), Status::Ok);
  e.submit(0, frame(store, 0));
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  e.takeResult(0).reset();
  fpe::test::resetAllocationCount();
  std::uint64_t ok = 0;
  EXPECT_NO_ALLOCATIONS({
    for (std::uint64_t i = 1; i <= 1000; ++i) {
      if (FrameRef f = frame(store, i)) e.submit(0, std::move(f));
      if (FrameResult r = e.takeResult(0)) ok += r.get<Value>(b)->v == r.info().frameId + 1 ? 1u : 0u;
      std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
  });
  EXPECT_EQ(fpe::test::allocationCount(), 0);
  e.stop();
  EXPECT_GT(ok, 0u);
  EXPECT_EQ(store.inUse(), 0u);
}
