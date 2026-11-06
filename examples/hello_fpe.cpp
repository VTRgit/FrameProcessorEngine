// Two synthetic 60 fps camera streams, three tasks per frame (two in parallel, one combining
// them), results taken by a consumer that is slower than the cameras. Prints fps, latency and
// drops per stream.

#include <fpe/engine.hpp>
#include <fpe/version.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>

namespace {

constexpr std::uint32_t kWidth = 320;
constexpr std::uint32_t kHeight = 240;

std::int64_t nowNs() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

struct Brightness {
  double mean = 0;
};
struct EdgeCount {
  std::uint32_t edges = 0;
};
struct Summary {
  double brightness = 0;
  std::uint32_t edges = 0;
};

void fill(std::uint8_t* px, std::uint32_t stream, std::uint64_t frame) {
  for (std::uint32_t y = 0; y < kHeight; ++y) {
    for (std::uint32_t x = 0; x < kWidth; ++x) {
      std::uint8_t v = 0;
      if (stream == 0) { // moving gradient
        v = static_cast<std::uint8_t>((x + frame * 4) & 0xFF);
      } else { // moving checkerboard
        v = (((x + frame) / 16 + y / 16) & 1) != 0 ? 230 : 25;
      }
      px[y * kWidth + x] = v;
    }
  }
}

} // namespace

int main() {
  std::printf("FrameProcessorEngine %d.%d.%d\n", fpe::version.major, fpe::version.minor,
              fpe::version.patch);

  fpe::FrameStore store({.poolSlots = 12, .poolSlotBytes = kWidth * kHeight, .externalSlots = 0});
  fpe::Engine engine(store, {.workers = 3, .streams = 2, .maxFramesInFlight = 6});

  const fpe::TaskId brightness = engine.addTask<Brightness>(
      "brightness", [](const fpe::TaskInput& in, Brightness& out) {
        const auto& info = in.info();
        const std::uint8_t* px = in.planes().data[0];
        std::uint64_t sum = 0;
        for (std::uint32_t i = 0; i < info.width * info.height; ++i) {
          sum += px[i];
        }
        out.mean = static_cast<double>(sum) / (info.width * info.height);
      });
  const fpe::TaskId edges = engine.addTask<EdgeCount>(
      "edges", [](const fpe::TaskInput& in, EdgeCount& out) {
        const auto& info = in.info();
        const std::uint8_t* px = in.planes().data[0];
        for (std::uint32_t y = 0; y < info.height; ++y) {
          for (std::uint32_t x = 1; x < info.width; ++x) {
            const int d = px[y * info.width + x] - px[y * info.width + x - 1];
            out.edges += (d > 32 || d < -32) ? 1u : 0u;
          }
        }
      });
  const fpe::TaskId summary = engine.addTask<Summary>(
      "summary",
      [brightness, edges](const fpe::TaskInput& in, Summary& out) {
        out.brightness = in.input<Brightness>(brightness).mean;
        out.edges = in.input<EdgeCount>(edges).edges;
      },
      {brightness, edges});

  if (fpe::Status s = engine.start(); s != fpe::Status::Ok) {
    std::printf("start failed: %s %s\n", fpe::toString(s), engine.errorMessage().c_str());
    return 1;
  }

  constexpr auto kDuration = std::chrono::seconds(2);
  std::atomic<bool> stop{false};
  std::vector<std::thread> cameras;
  for (std::uint32_t s = 0; s < 2; ++s) {
    cameras.emplace_back([&, s] {
      auto next = std::chrono::steady_clock::now();
      for (std::uint64_t i = 1; !stop.load(std::memory_order_relaxed); ++i) {
        fpe::FrameHandle h;
        if (store.acquire(h) == fpe::Status::Ok) {
          auto* info = store.mutableInfo(h);
          *info = {.streamId = s, .frameId = i, .timestampNs = nowNs(), .width = kWidth,
                   .height = kHeight, .format = fpe::PixelFormat::Gray8};
          store.mutablePlanes(h)->stride[0] = kWidth;
          fill(store.poolMemory(h), s, i);
          engine.submit(s, fpe::FrameRef(store, h));
        }
        next += std::chrono::microseconds(16667);
        std::this_thread::sleep_until(next);
      }
    });
  }

  std::uint64_t results[2]{};
  double latencyMs[2]{};
  Summary last[2]{};
  const auto end = std::chrono::steady_clock::now() + kDuration;
  while (std::chrono::steady_clock::now() < end) {
    for (std::uint32_t s = 0; s < 2; ++s) {
      if (fpe::FrameResult r = engine.takeResult(s)) {
        latencyMs[s] += static_cast<double>(nowNs() - r.info().timestampNs) / 1e6;
        last[s] = *r.get<Summary>(summary);
        ++results[s];
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(25)); // a 40 Hz consumer
  }
  stop.store(true);
  for (auto& c : cameras) {
    c.join();
  }

  for (std::uint32_t s = 0; s < 2; ++s) {
    const fpe::StreamStats st = engine.stats(s);
    const double seconds = std::chrono::duration<double>(kDuration).count();
    std::printf("stream %u: submitted %llu, processed %.1f fps, consumer got %.1f fps, "
                "mean latency %.2f ms, dropped %llu (input %llu, no job %llu, results %llu), "
                "last brightness %.1f edges %u\n",
                s, static_cast<unsigned long long>(st.submitted),
                static_cast<double>(st.completed) / seconds,
                static_cast<double>(results[s]) / seconds,
                results[s] ? latencyMs[s] / static_cast<double>(results[s]) : 0.0,
                static_cast<unsigned long long>(st.replacedInput + st.droppedNoJob + st.replacedResults),
                static_cast<unsigned long long>(st.replacedInput),
                static_cast<unsigned long long>(st.droppedNoJob),
                static_cast<unsigned long long>(st.replacedResults), last[s].brightness,
                last[s].edges);
  }
  engine.stop();
  return 0;
}
