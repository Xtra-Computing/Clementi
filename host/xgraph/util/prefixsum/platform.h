#ifndef PLATFORM_H_
#define PLATFORM_H_

#include <pthread.h>

#include <cassert>
#include <thread>
#include <vector>

#define NUM_HARDWARE_THREADS (CountHardwareThreads())

// TODO: Set the max to 256 for now, used for internal data structure sizes.
constexpr int MAX_NUM_THREADS{256};

// Returns the number of hardware threads.
inline int CountHardwareThreads() {
  return static_cast<int>(std::thread::hardware_concurrency());
}

class ThreadScheduler {
 public:
  static int GetPhysicalCPU(const int cpu_id) {
    if (cpu_mapping_.empty()) return cpu_id;
    assert(cpu_id < cpu_mapping_.size());
    return cpu_mapping_[cpu_id];
  }

  // By default <cpu_mapping_> is empty and use <cpu_id> as the physical id.
  static inline std::vector<int> cpu_mapping_ = {};
};

// Pins <thread> to the CPU <cpu_id>.
inline void BindCpu(int cpu_id, pthread_t thread = pthread_self()) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(ThreadScheduler::GetPhysicalCPU(cpu_id), &cpuset);
  const int ret{pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset)};
  // ASSERT(ret == 0, "pthread_setaffinity_np");
}

#endif  // PLATFORM_H_