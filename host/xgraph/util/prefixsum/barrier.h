#ifndef BARRIER_H_
#define BARRIER_H_

#include <atomic>

// https://en.cppreference.com/w/cpp/thread/barrier
// https://cs61.seas.harvard.edu/site/2019/SynchEx

class spinlock_barrier {
 public:
  spinlock_barrier(const spinlock_barrier&) = delete;
  spinlock_barrier& operator=(const spinlock_barrier&) = delete;

  explicit spinlock_barrier(unsigned int n) : n_{n}, k_{0}, sense_{false} {}

  void arrive_and_wait() {
    bool my_sense = this->sense_.load();
    if (++this->k_ == this->n_) {
      this->k_ = 0;
      this->sense_ = !my_sense;
    } else {
      while (this->sense_.load() == my_sense) {
        // TODO: Check the effect of yield() in this busy wait.
      }
    }
  }

 private:
  unsigned int n_;
  std::atomic<unsigned int> k_;
  std::atomic<bool> sense_;
};

#endif  // BARRIER_H_
