#ifndef CBD9C3E6_CD41_43EC_A6F3_27BC9E1E8213
#define CBD9C3E6_CD41_43EC_A6F3_27BC9E1E8213

#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemWithBarrier.h>
#include <Jolt/Core/FixedSizeFreeList.h>
#include <shards/core/async.hpp>

namespace shards::Physics {

struct TidePoolJobSystem : public JPH::JobSystemWithBarrier {
  struct TidePoolJob : public Job, public TidePool::Work {
    TidePoolJob(const char *inJobName, JPH::ColorArg inColor, JobSystem *inJobSystem, const JobFunction &inJobFunction,
                uint32_t inNumDependencies)
        : Job(inJobName, inColor, inJobSystem, inJobFunction, inNumDependencies) {}
    void call() override {
      ZoneScopedN("Physics::JobWork");
      Execute();
    }
  };
  JPH::FixedSizeFreeList<TidePoolJob> jobs;

  TidePoolJobSystem() : JobSystemWithBarrier(2) {
    const int maxJobs = 2048;
    jobs.Init(maxJobs, maxJobs);
  }

  virtual int GetMaxConcurrency() const {
#if SH_ENABLE_TIDE_POOL
    return getTidePool().MaxWorkers;
#else
    return 2;
#endif
  }

  /// Create a new job, the job is started immediately if inNumDependencies == 0 otherwise it starts when
  /// RemoveDependency causes the dependency counter to reach 0.
  virtual JPH::JobHandle CreateJob(const char *inName, JPH::ColorArg inColor, const JobFunction &inJobFunction,
                                   uint32_t inNumDependencies = 0) {
    int jobIndex = jobs.ConstructObject(inName, inColor, this, inJobFunction, inNumDependencies);
    shassert(jobIndex != decltype(jobs)::cInvalidObjectIndex);

    auto *job = &jobs.Get(jobIndex);
    JobHandle handle(job);
    if (inNumDependencies == 0)
      QueueJob(job);
    return handle;
    // return JobHandle(new TidePoolJob(inName, inColor, this, inJobFunction, inNumDependencies));
  }

  /// Adds a job to the job queue
  virtual void QueueJob(Job *inJob) {
    auto *job = static_cast<TidePoolJob *>(inJob);
    shards::getTidePool().schedule(static_cast<TidePool::Work *>(job));
  }

  /// Adds a number of jobs at once to the job queue
  virtual void QueueJobs(Job **inJobs, uint32_t inNumJobs) {
    for (uint32_t i = 0; i < inNumJobs; ++i) {
      QueueJob(inJobs[i]);
    }
  }

  /// Frees a job

  virtual void FreeJob(Job *inJob) {
    auto *job = static_cast<TidePoolJob *>(inJob);
    jobs.DestructObject(job);
  }
};
} // namespace shards::Physics

#endif /* CBD9C3E6_CD41_43EC_A6F3_27BC9E1E8213 */
