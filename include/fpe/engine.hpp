#pragma once

#include <fpe/frame.hpp>
#include <fpe/latest_mailbox.hpp>
#include <fpe/status.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace fpe {

namespace detail {
class IndexStack;
template <class T> class MpmcQueue;
template <class T> struct TypeTag {
  static constexpr char id = 0;
};
} // namespace detail

using TaskId = std::uint16_t;
inline constexpr TaskId kInvalidTask = 0xFFFF;

/// Largest task output, in bytes. Outputs live in preallocated per-frame storage.
inline constexpr std::size_t kMaxTaskOutputBytes = 4096;
inline constexpr std::size_t kMaxTaskOutputAlign = 64;
inline constexpr std::size_t kMaxTasks = 256;

enum class Mode : std::uint8_t {
  RealTime, ///< Latest frame wins; frames and results may be dropped (and are counted).
  Offline,  ///< process() runs every frame in order; nothing is dropped.
};

enum class TaskStatus : std::uint8_t { Pending, Ok, Failed, Skipped };

class Engine;
class FrameResult;

/// Called once on each worker thread when it starts (set priority, affinity, test hooks...).
struct WorkerHook {
  void (*fn)(std::uint32_t workerIndex, void* context) noexcept = nullptr;
  void* context = nullptr;
};

/// Real-time results can be delivered to a callback (on a worker thread) instead of the
/// per-stream result mailboxes. The callee owns the result and must release it quickly.
struct ResultCallback {
  void (*fn)(FrameResult&& result, void* context) noexcept = nullptr;
  void* context = nullptr;
};

struct EngineConfig {
  std::uint32_t workers = 2;
  std::uint32_t streams = 1;
  /// Frames being processed plus results not yet released by the consumer.
  std::uint32_t maxFramesInFlight = 4;
  Mode mode = Mode::RealTime;
  WorkerHook onWorkerStart{};
  ResultCallback onResult{};
};

struct StreamStats {
  std::uint64_t submitted = 0;       ///< Frames handed to submit().
  std::uint64_t replacedInput = 0;   ///< Frames replaced by a newer one before processing started.
  std::uint64_t droppedNoJob = 0;    ///< Frames dropped because every frame job was in use.
  std::uint64_t completed = 0;       ///< Frames fully processed.
  std::uint64_t replacedResults = 0; ///< Results replaced by a newer one before the consumer took them.
};

/// What a task sees: the frame and the outputs of the tasks it depends on.
class TaskInput {
public:
  const FrameInfo& info() const noexcept;
  const Planes& planes() const noexcept;
  FrameHandle frame() const noexcept;
  const FrameStore& store() const noexcept;
  std::uint32_t stream() const noexcept;

  /// Output of a dependency. `dep` must be a declared dependency producing a `T`.
  template <class T> const T& input(TaskId dep) const noexcept;

private:
  friend class Engine;
  TaskInput(const Engine& e, std::uint32_t job, TaskId self) noexcept
      : engine_(&e), job_(job), self_(self) {}
  const Engine* engine_;
  std::uint32_t job_;
  TaskId self_;
};

/// One processed frame: the frame plus every task's status and output. Move-only; releasing it
/// (destructor or reset()) returns the frame and the result storage to their pools. A FrameResult
/// must not outlive its Engine.
class FrameResult {
public:
  FrameResult() noexcept = default;
  FrameResult(FrameResult&& o) noexcept : engine_(o.engine_), job_(o.job_), generation_(o.generation_) {
    o.engine_ = nullptr;
  }
  FrameResult& operator=(FrameResult&& o) noexcept;
  FrameResult(const FrameResult&) = delete;
  FrameResult& operator=(const FrameResult&) = delete;
  ~FrameResult() { reset(); }

  explicit operator bool() const noexcept { return engine_ != nullptr; }
  void reset() noexcept;

  std::uint32_t stream() const noexcept;
  const FrameInfo& info() const noexcept;
  FrameHandle frame() const noexcept;
  TaskStatus status(TaskId task) const noexcept;
  /// nullptr if the task did not succeed or `T` is not its output type.
  template <class T> const T* get(TaskId task) const noexcept;

private:
  friend class Engine;
  FrameResult(Engine& e, std::uint32_t job, std::uint32_t generation) noexcept
      : engine_(&e), job_(job), generation_(generation) {}
  Engine* engine_ = nullptr;
  std::uint32_t job_ = 0;
  std::uint32_t generation_ = 0;
};

class Engine {
public:
  Engine(FrameStore& store, const EngineConfig& config);
  ~Engine();
  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  /// Registers a task before start(). `fn` is called as `fn(const TaskInput&, Output&)` and
  /// returns `bool` (false = failed) or `void` (always succeeds). It must be safe to call
  /// concurrently for different frames and must not throw.
  /// Errors (duplicate name, unknown dependency, after start) are reported by start().
  template <class Output, class F>
  TaskId addTask(std::string_view name, F&& fn, std::initializer_list<TaskId> dependsOn = {});

  /// Adds a dependency after both tasks exist (allows forward references; cycles are rejected
  /// by start()).
  Status addDependency(TaskId task, TaskId dependsOn);

  /// Validates the graph, allocates everything and starts the workers.
  Status start();
  /// Finishes frames being processed, releases pending frames and results, joins the workers.
  void stop();
  bool running() const noexcept { return running_.load(std::memory_order_acquire); }
  /// Human-readable detail of the last start() error.
  const std::string& errorMessage() const noexcept { return error_; }

  // --- Real-time mode -------------------------------------------------------------------------
  /// Hands a frame to a stream. Call from one producer thread per stream. Never blocks.
  Status submit(std::uint32_t stream, FrameRef&& frame) noexcept;
  /// Newest result of a stream not yet taken (empty FrameResult if none).
  FrameResult takeResult(std::uint32_t stream) noexcept;

  // --- Offline mode ---------------------------------------------------------------------------
  /// Processes one frame and waits for its result. Frames are processed in call order.
  Status process(std::uint32_t stream, FrameRef&& frame, FrameResult& out) noexcept;

  StreamStats stats(std::uint32_t stream) const noexcept;
  std::size_t taskCount() const noexcept { return tasks_.size(); }
  std::uint32_t workerCount() const noexcept { return static_cast<std::uint32_t>(workers_.size()); }
  const FrameStore& store() const noexcept { return store_; }

private:
  friend class TaskInput;
  friend class FrameResult;

  struct Callable {
    virtual ~Callable() = default;
    virtual void init(void* out) const noexcept = 0;
    virtual bool run(const TaskInput& in, void* out) const noexcept = 0;
  };
  template <class Output, class F> struct CallableImpl final : Callable {
    explicit CallableImpl(F&& f) : fn(std::forward<F>(f)) {}
    void init(void* out) const noexcept override { ::new (out) Output{}; }
    bool run(const TaskInput& in, void* out) const noexcept override {
      Output& o = *std::launder(static_cast<Output*>(out));
      if constexpr (std::is_same_v<decltype(fn(in, o)), void>) {
        fn(in, o);
        return true;
      } else {
        return static_cast<bool>(fn(in, o));
      }
    }
    std::decay_t<F> fn;
  };

  struct TaskSpec {
    std::string name;
    std::vector<TaskId> deps;
    std::size_t size = 0;
    std::size_t align = 1;
    const void* type = nullptr;
    std::unique_ptr<Callable> callable;
  };

  struct Job;
  struct Stream;

  TaskId registerTask(std::string_view name, std::initializer_list<TaskId> deps, std::size_t size,
                      std::size_t align, const void* type, std::unique_ptr<Callable> callable);
  void workerLoop(std::uint32_t index) noexcept;
  void runItem(std::uint32_t item) noexcept;
  bool startJob(std::uint32_t stream, FrameHandle frame, std::uint32_t& jobOut) noexcept;
  void finishJob(std::uint32_t job) noexcept;
  void tryDispatch(std::uint32_t stream) noexcept;
  void releaseResult(std::uint32_t job, std::uint32_t generation) noexcept;
  void enqueue(std::uint32_t job, TaskId task) noexcept;

  void* outputPtr(std::uint32_t job, TaskId task) const noexcept;
  TaskStatus taskStatus(std::uint32_t job, TaskId task) const noexcept;
  const Job& job(std::uint32_t j) const noexcept;
  bool isDependency(TaskId task, TaskId dep) const noexcept;

  FrameStore& store_;
  EngineConfig config_;
  std::vector<TaskSpec> tasks_;
  Status pendingError_ = Status::Ok;
  std::string error_;

  // Built by start().
  std::vector<std::uint16_t> depCount_;
  std::vector<std::uint32_t> dependentsBegin_; // CSR: dependents of task t are
  std::vector<TaskId> dependents_;             // dependents_[begin[t] .. begin[t+1])
  std::vector<TaskId> roots_;
  std::vector<std::size_t> outputOffset_;
  std::size_t jobOutputBytes_ = 0;

  std::unique_ptr<Job[]> jobs_;
  std::unique_ptr<std::atomic<std::uint16_t>[]> remaining_;
  std::unique_ptr<std::atomic<std::uint8_t>[]> status_;
  std::byte* outputs_ = nullptr;
  std::unique_ptr<detail::IndexStack> freeJobs_;
  std::unique_ptr<detail::MpmcQueue<std::uint32_t>> queue_;
  std::unique_ptr<Stream[]> streams_;
  std::vector<std::thread> workers_;

  std::atomic<bool> running_{false};
  std::atomic<bool> accepting_{false};
  std::atomic<bool> workersRun_{false};
  std::atomic<std::uint32_t> signal_{0};
  std::atomic<std::uint32_t> inFlight_{0};
};

// ---- templates -------------------------------------------------------------------------------

template <class Output, class F>
TaskId Engine::addTask(std::string_view name, F&& fn, std::initializer_list<TaskId> dependsOn) {
  static_assert(std::is_trivially_copyable_v<Output>, "task outputs must be trivially copyable");
  static_assert(std::is_default_constructible_v<Output>, "task outputs must be default constructible");
  static_assert(sizeof(Output) <= kMaxTaskOutputBytes, "task output exceeds kMaxTaskOutputBytes");
  static_assert(alignof(Output) <= kMaxTaskOutputAlign, "task output alignment exceeds 64");
  static_assert(std::is_invocable_v<F&, const TaskInput&, Output&>,
                "task must be callable as fn(const TaskInput&, Output&)");
  return registerTask(name, dependsOn, sizeof(Output), alignof(Output), &detail::TypeTag<Output>::id,
                      std::make_unique<CallableImpl<Output, F>>(std::forward<F>(fn)));
}

template <class T> const T& TaskInput::input(TaskId dep) const noexcept {
  FPE_ASSERT(engine_->isDependency(self_, dep), "input() of a task that is not a declared dependency");
  FPE_ASSERT(engine_->tasks_[dep].type == &detail::TypeTag<T>::id, "input() with the wrong output type");
  return *std::launder(static_cast<const T*>(engine_->outputPtr(job_, dep)));
}

template <class T> const T* FrameResult::get(TaskId task) const noexcept {
  if (engine_ == nullptr || task >= engine_->tasks_.size() ||
      engine_->tasks_[task].type != &detail::TypeTag<T>::id ||
      engine_->taskStatus(job_, task) != TaskStatus::Ok) {
    return nullptr;
  }
  return std::launder(static_cast<const T*>(engine_->outputPtr(job_, task)));
}

} // namespace fpe
