#include <fpe/detail/index_stack.hpp>
#include <fpe/detail/mpmc_queue.hpp>
#include <fpe/engine.hpp>
#include <fpe/version.hpp>

#include <algorithm>
#include <chrono>

namespace fpe {

struct Engine::Job {
  FrameHandle frame{};
  std::uint32_t stream = 0;
  std::atomic<std::uint32_t> tasksLeft{0};
  std::atomic<std::uint32_t> generation{1};
  std::atomic<std::uint32_t> done{0};
};

struct Engine::Stream {
  LatestMailbox<FrameHandle> input;
  LatestMailbox<std::uint64_t> results; // (generation << 32) | job
  std::atomic<bool> busy{false};
  std::atomic<std::uint64_t> submitted{0};
  std::atomic<std::uint64_t> droppedNoJob{0};
  std::atomic<std::uint64_t> completed{0};
};

namespace {
constexpr std::uint64_t packResult(std::uint32_t job, std::uint32_t gen) {
  return (static_cast<std::uint64_t>(gen) << 32) | job;
}
constexpr std::uint32_t resultJob(std::uint64_t v) { return static_cast<std::uint32_t>(v & 0xFFFFFFFFu); }
constexpr std::uint32_t resultGen(std::uint64_t v) { return static_cast<std::uint32_t>(v >> 32); }
std::size_t alignUp(std::size_t v, std::size_t a) { return (v + a - 1) / a * a; }
void stampReceived(FrameStore& store, FrameHandle h) noexcept {
  if (FrameInfo* info = store.mutableInfo(h); info != nullptr && info->receivedNs == 0) {
    info->receivedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count();
  }
}
} // namespace

// ---- TaskInput -------------------------------------------------------------------------------

const FrameInfo& TaskInput::info() const noexcept {
  return *engine_->store_.info(engine_->job(job_).frame);
}
const Planes& TaskInput::planes() const noexcept {
  return *engine_->store_.planes(engine_->job(job_).frame);
}
FrameHandle TaskInput::frame() const noexcept { return engine_->job(job_).frame; }
const FrameStore& TaskInput::store() const noexcept { return engine_->store_; }
std::uint32_t TaskInput::stream() const noexcept { return engine_->job(job_).stream; }

// ---- FrameResult -----------------------------------------------------------------------------

FrameResult& FrameResult::operator=(FrameResult&& o) noexcept {
  if (this != &o) {
    reset();
    engine_ = o.engine_;
    job_ = o.job_;
    generation_ = o.generation_;
    o.engine_ = nullptr;
  }
  return *this;
}

void FrameResult::reset() noexcept {
  if (engine_ != nullptr) {
    engine_->releaseResult(job_, generation_);
    engine_ = nullptr;
  }
}

std::uint32_t FrameResult::stream() const noexcept { return engine_->job(job_).stream; }
const FrameInfo& FrameResult::info() const noexcept {
  return *engine_->store_.info(engine_->job(job_).frame);
}
FrameHandle FrameResult::frame() const noexcept { return engine_->job(job_).frame; }
TaskStatus FrameResult::status(TaskId task) const noexcept {
  if (engine_ == nullptr || task >= engine_->tasks_.size()) {
    return TaskStatus::Pending;
  }
  return engine_->taskStatus(job_, task);
}

// ---- Engine: registration --------------------------------------------------------------------

Engine::Engine(FrameStore& store, const EngineConfig& config) : store_(store), config_(config) {}

Engine::~Engine() {
  stop();
  if (outputs_ != nullptr) {
    ::operator delete(outputs_, std::align_val_t{kMaxTaskOutputAlign});
  }
}

TaskId Engine::registerTask(std::string_view name, std::initializer_list<TaskId> deps,
                            std::size_t size, std::size_t align, const void* type,
                            std::unique_ptr<Callable> callable) {
  auto fail = [&](Status s, std::string msg) {
    if (pendingError_ == Status::Ok) {
      pendingError_ = s;
      error_ = std::move(msg);
    }
    return kInvalidTask;
  };
  if (running()) {
    return fail(Status::AlreadyStarted, "addTask after start()");
  }
  if (tasks_.size() >= kMaxTasks) {
    return fail(Status::TooManyTasks, "more than kMaxTasks tasks");
  }
  for (const auto& t : tasks_) {
    if (t.name == name) {
      return fail(Status::DuplicateTask, "duplicate task name '" + std::string(name) + "'");
    }
  }
  for (TaskId d : deps) {
    if (d >= tasks_.size()) {
      return fail(Status::UnknownDependency,
                  "task '" + std::string(name) + "' depends on an unknown task id");
    }
  }
  TaskSpec spec;
  spec.name = std::string(name);
  spec.deps.assign(deps.begin(), deps.end());
  std::sort(spec.deps.begin(), spec.deps.end());
  spec.deps.erase(std::unique(spec.deps.begin(), spec.deps.end()), spec.deps.end());
  spec.size = size;
  spec.align = align;
  spec.type = type;
  spec.callable = std::move(callable);
  tasks_.push_back(std::move(spec));
  return static_cast<TaskId>(tasks_.size() - 1);
}

Status Engine::addDependency(TaskId task, TaskId dependsOn) {
  if (running()) {
    return Status::AlreadyStarted;
  }
  if (task >= tasks_.size() || dependsOn >= tasks_.size()) {
    return Status::UnknownDependency;
  }
  auto& deps = tasks_[task].deps;
  if (std::find(deps.begin(), deps.end(), dependsOn) == deps.end()) {
    deps.push_back(dependsOn);
    std::sort(deps.begin(), deps.end());
  }
  return Status::Ok;
}

bool Engine::isDependency(TaskId task, TaskId dep) const noexcept {
  const auto& deps = tasks_[task].deps;
  return std::binary_search(deps.begin(), deps.end(), dep);
}

// ---- Engine: start / stop --------------------------------------------------------------------

Status Engine::start() {
  if (running()) {
    return Status::AlreadyStarted;
  }
  if (pendingError_ != Status::Ok) {
    return pendingError_;
  }
  if (config_.workers == 0 || config_.streams == 0 || config_.maxFramesInFlight == 0 ||
      config_.maxFramesInFlight > 0xFFFFu) {
    error_ = "workers, streams and maxFramesInFlight must be > 0 (maxFramesInFlight <= 65535)";
    return Status::InvalidArgument;
  }

  const std::size_t n = tasks_.size();
  // Cycle detection (Kahn) and dependency counts.
  depCount_.assign(n, 0);
  std::vector<std::vector<TaskId>> dependents(n);
  for (std::size_t t = 0; t < n; ++t) {
    depCount_[t] = static_cast<std::uint16_t>(tasks_[t].deps.size());
    for (TaskId d : tasks_[t].deps) {
      dependents[d].push_back(static_cast<TaskId>(t));
    }
  }
  {
    std::vector<std::uint16_t> left = depCount_;
    std::vector<TaskId> ready;
    for (std::size_t t = 0; t < n; ++t) {
      if (left[t] == 0) {
        ready.push_back(static_cast<TaskId>(t));
      }
    }
    std::size_t visited = 0;
    while (!ready.empty()) {
      TaskId t = ready.back();
      ready.pop_back();
      ++visited;
      for (TaskId d : dependents[t]) {
        if (--left[d] == 0) {
          ready.push_back(d);
        }
      }
    }
    if (visited != n) {
      error_ = "task graph has a cycle involving:";
      for (std::size_t t = 0; t < n; ++t) {
        if (left[t] != 0) {
          error_ += " '" + tasks_[t].name + "'";
        }
      }
      return Status::GraphCycle;
    }
  }

  dependentsBegin_.assign(n + 1, 0);
  dependents_.clear();
  roots_.clear();
  for (std::size_t t = 0; t < n; ++t) {
    dependentsBegin_[t] = static_cast<std::uint32_t>(dependents_.size());
    dependents_.insert(dependents_.end(), dependents[t].begin(), dependents[t].end());
    if (depCount_[t] == 0) {
      roots_.push_back(static_cast<TaskId>(t));
    }
  }
  dependentsBegin_[n] = static_cast<std::uint32_t>(dependents_.size());

  outputOffset_.assign(n, 0);
  std::size_t offset = 0;
  for (std::size_t t = 0; t < n; ++t) {
    offset = alignUp(offset, tasks_[t].align);
    outputOffset_[t] = offset;
    offset += std::max<std::size_t>(tasks_[t].size, 1);
  }
  jobOutputBytes_ = alignUp(std::max<std::size_t>(offset, 1), kMaxTaskOutputAlign);

  const std::uint32_t jobs = config_.maxFramesInFlight;
  jobs_ = std::make_unique<Job[]>(jobs);
  remaining_ = std::make_unique<std::atomic<std::uint16_t>[]>(std::max<std::size_t>(jobs * n, 1));
  status_ = std::make_unique<std::atomic<std::uint8_t>[]>(std::max<std::size_t>(jobs * n, 1));
  if (outputs_ != nullptr) {
    ::operator delete(outputs_, std::align_val_t{kMaxTaskOutputAlign});
  }
  outputs_ = static_cast<std::byte*>(
      ::operator new(jobOutputBytes_ * jobs, std::align_val_t{kMaxTaskOutputAlign}));
  freeJobs_ = std::make_unique<detail::IndexStack>(jobs);
  for (std::uint32_t j = jobs; j-- > 0;) {
    freeJobs_->push(j);
  }
  queue_ = std::make_unique<detail::MpmcQueue<std::uint32_t>>(std::max<std::size_t>(jobs * n, 2));
  streams_ = std::make_unique<Stream[]>(config_.streams);

  signal_.store(0, std::memory_order_relaxed);
  inFlight_.store(0, std::memory_order_relaxed);
  workersRun_.store(true, std::memory_order_release);
  workers_.reserve(config_.workers);
  for (std::uint32_t i = 0; i < config_.workers; ++i) {
    workers_.emplace_back([this, i] { workerLoop(i); });
  }
  error_.clear();
  accepting_.store(true, std::memory_order_release);
  running_.store(true, std::memory_order_release);
  return Status::Ok;
}

void Engine::stop() {
  if (!running()) {
    return;
  }
  accepting_.store(false, std::memory_order_release);
  // Let frames already being processed finish.
  while (inFlight_.load(std::memory_order_acquire) != 0) {
    std::this_thread::sleep_for(std::chrono::microseconds(100));
  }
  workersRun_.store(false, std::memory_order_release);
  signal_.fetch_add(1, std::memory_order_release);
  signal_.notify_all();
  for (auto& w : workers_) {
    w.join();
  }
  workers_.clear();
  // Release frames that never started and results nobody took.
  for (std::uint32_t s = 0; s < config_.streams; ++s) {
    if (auto f = streams_[s].input.take()) {
      store_.release(*f);
    }
    if (auto r = streams_[s].results.take()) {
      releaseResult(resultJob(*r), resultGen(*r));
    }
  }
  running_.store(false, std::memory_order_release);
}

// ---- Engine: frame path ----------------------------------------------------------------------

const Engine::Job& Engine::job(std::uint32_t j) const noexcept { return jobs_[j]; }

void* Engine::outputPtr(std::uint32_t job, TaskId task) const noexcept {
  return outputs_ + job * jobOutputBytes_ + outputOffset_[task];
}

TaskStatus Engine::taskStatus(std::uint32_t job, TaskId task) const noexcept {
  return static_cast<TaskStatus>(status_[job * tasks_.size() + task].load(std::memory_order_acquire));
}

void Engine::enqueue(std::uint32_t job, TaskId task) noexcept {
  const bool ok = queue_->push((job << 16) | task);
  FPE_ASSERT(ok, "work queue full (cannot happen: one slot per job and task)");
  (void)ok;
  signal_.fetch_add(1, std::memory_order_release);
  signal_.notify_one();
}

bool Engine::startJob(std::uint32_t stream, FrameHandle frame, std::uint32_t& jobOut) noexcept {
  const std::uint32_t j = freeJobs_->pop();
  if (j == detail::IndexStack::npos) {
    return false;
  }
  Job& jb = jobs_[j];
  const std::size_t n = tasks_.size();
  jb.frame = frame;
  jb.stream = stream;
  jb.done.store(0, std::memory_order_relaxed);
  jb.tasksLeft.store(static_cast<std::uint32_t>(n), std::memory_order_relaxed);
  for (std::size_t t = 0; t < n; ++t) {
    remaining_[j * n + t].store(depCount_[t], std::memory_order_relaxed);
    status_[j * n + t].store(static_cast<std::uint8_t>(TaskStatus::Pending), std::memory_order_relaxed);
    tasks_[t].callable->init(outputPtr(j, static_cast<TaskId>(t)));
  }
  inFlight_.fetch_add(1, std::memory_order_acq_rel);
  jobOut = j;
  if (n == 0) {
    finishJob(j);
    return true;
  }
  for (TaskId r : roots_) {
    enqueue(j, r);
  }
  return true;
}

void Engine::workerLoop(std::uint32_t index) noexcept {
  if (config_.onWorkerStart.fn != nullptr) {
    config_.onWorkerStart.fn(index, config_.onWorkerStart.context);
  }
  std::uint32_t item = 0;
  for (;;) {
    if (queue_->pop(item)) {
      runItem(item);
      continue;
    }
    const std::uint32_t seen = signal_.load(std::memory_order_acquire);
    if (queue_->pop(item)) {
      runItem(item);
      continue;
    }
    if (!workersRun_.load(std::memory_order_acquire)) {
      return;
    }
    signal_.wait(seen, std::memory_order_acquire);
  }
}

void Engine::runItem(std::uint32_t item) noexcept {
  const std::uint32_t j = item >> 16;
  const auto t = static_cast<TaskId>(item & 0xFFFFu);
  const std::size_t n = tasks_.size();

  bool depsOk = true;
  for (TaskId d : tasks_[t].deps) {
    if (taskStatus(j, d) != TaskStatus::Ok) {
      depsOk = false;
      break;
    }
  }
  TaskStatus result = TaskStatus::Skipped;
  if (depsOk) {
    TaskInput in(*this, j, t);
    result = tasks_[t].callable->run(in, outputPtr(j, t)) ? TaskStatus::Ok : TaskStatus::Failed;
  }
  status_[j * n + t].store(static_cast<std::uint8_t>(result), std::memory_order_release);

  for (std::uint32_t k = dependentsBegin_[t]; k < dependentsBegin_[t + 1u]; ++k) {
    const TaskId d = dependents_[k];
    if (remaining_[j * n + d].fetch_sub(1, std::memory_order_acq_rel) == 1) {
      enqueue(j, d);
    }
  }
  if (jobs_[j].tasksLeft.fetch_sub(1, std::memory_order_acq_rel) == 1) {
    finishJob(j);
  }
}

void Engine::finishJob(std::uint32_t j) noexcept {
  Job& jb = jobs_[j];
  const std::uint32_t s = jb.stream;
  const std::uint32_t gen = jb.generation.load(std::memory_order_relaxed);
  Stream& st = streams_[s];
  st.completed.fetch_add(1, std::memory_order_relaxed);

  if (config_.mode == Mode::Offline) {
    inFlight_.fetch_sub(1, std::memory_order_acq_rel);
    jb.done.store(1, std::memory_order_release);
    jb.done.notify_all();
    return;
  }

  if (config_.onResult.fn != nullptr) {
    config_.onResult.fn(FrameResult(*this, j, gen), config_.onResult.context);
  } else if (auto displaced = st.results.publish(packResult(j, gen))) {
    releaseResult(resultJob(*displaced), resultGen(*displaced));
  }
  inFlight_.fetch_sub(1, std::memory_order_acq_rel);
  st.busy.store(false, std::memory_order_release);
  tryDispatch(s);
}

void Engine::releaseResult(std::uint32_t j, std::uint32_t generation) noexcept {
  Job& jb = jobs_[j];
  std::uint32_t expected = generation;
  // The generation bump makes the release exactly-once even if a stale handle is released twice.
  if (!jb.generation.compare_exchange_strong(expected, generation + 1, std::memory_order_acq_rel)) {
    FPE_ASSERT(false, "stale frame result");
    return;
  }
  store_.release(jb.frame);
  jb.frame = {};
  freeJobs_->push(j);
}

void Engine::tryDispatch(std::uint32_t s) noexcept {
  Stream& st = streams_[s];
  for (;;) {
    bool expected = false;
    if (!st.busy.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
      return; // a frame of this stream is in flight; its completion dispatches the next one
    }
    if (accepting_.load(std::memory_order_acquire)) {
      if (auto f = st.input.take()) {
        std::uint32_t j = 0;
        if (startJob(s, *f, j)) {
          return; // busy stays set until finishJob
        }
        store_.release(*f);
        st.droppedNoJob.fetch_add(1, std::memory_order_relaxed);
        st.busy.store(false, std::memory_order_release);
        return; // every job is held; the next submit retries
      }
    }
    st.busy.store(false, std::memory_order_release);
    if (!st.input.hasPending() || !accepting_.load(std::memory_order_acquire)) {
      return;
    }
  }
}

Status Engine::submit(std::uint32_t stream, FrameRef&& frame) noexcept {
  if (!accepting_.load(std::memory_order_acquire)) {
    return Status::NotStarted;
  }
  if (config_.mode != Mode::RealTime || stream >= config_.streams || !frame ||
      frame.store() != &store_) {
    return Status::InvalidArgument;
  }
  stampReceived(store_, frame.handle());
  Stream& st = streams_[stream];
  st.submitted.fetch_add(1, std::memory_order_relaxed);
  if (auto displaced = st.input.publish(frame.detach())) {
    store_.release(*displaced);
  }
  tryDispatch(stream);
  return Status::Ok;
}

FrameResult Engine::takeResult(std::uint32_t stream) noexcept {
  if (!running() || stream >= config_.streams) {
    return {};
  }
  if (auto r = streams_[stream].results.take()) {
    return FrameResult(*this, resultJob(*r), resultGen(*r));
  }
  return {};
}

Status Engine::process(std::uint32_t stream, FrameRef&& frame, FrameResult& out) noexcept {
  if (!accepting_.load(std::memory_order_acquire)) {
    return Status::NotStarted;
  }
  if (config_.mode != Mode::Offline || stream >= config_.streams || !frame ||
      frame.store() != &store_) {
    return Status::InvalidArgument;
  }
  stampReceived(store_, frame.handle());
  std::uint32_t j = 0;
  if (!startJob(stream, frame.handle(), j)) {
    return Status::PoolExhausted; // frame still owned by the caller's FrameRef
  }
  frame.detach();
  streams_[stream].submitted.fetch_add(1, std::memory_order_relaxed);
  Job& jb = jobs_[j];
  while (jb.done.load(std::memory_order_acquire) == 0) {
    jb.done.wait(0, std::memory_order_acquire);
  }
  out = FrameResult(*this, j, jb.generation.load(std::memory_order_relaxed));
  return Status::Ok;
}

StreamStats Engine::stats(std::uint32_t stream) const noexcept {
  StreamStats s;
  if (!streams_ || stream >= config_.streams) {
    return s;
  }
  const Stream& st = streams_[stream];
  s.submitted = st.submitted.load(std::memory_order_relaxed);
  s.replacedInput = st.input.replaced();
  s.droppedNoJob = st.droppedNoJob.load(std::memory_order_relaxed);
  s.completed = st.completed.load(std::memory_order_relaxed);
  s.replacedResults = st.results.replaced();
  return s;
}

} // namespace fpe

namespace fpe::detail {
int versionMajor() noexcept { return version.major; }
} // namespace fpe::detail
