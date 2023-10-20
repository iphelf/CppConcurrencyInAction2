//
// Created by iphelf on 2023-10-08.
//

#include <atomic>
#include <cassert>
#include <latch>
#include <mutex>
#include <thread>

class spinlock_mutex {
  std::atomic_flag flag;

 public:
  void lock() {
    while (flag.test_and_set()) std::this_thread::yield();
  }
  void unlock() { flag.clear(); }
};

int main() {
  spinlock_mutex mutex;
  std::latch latch{2};
  const int n_ops{1'000'000};
  int count{0};
  std::thread thread{[&] {
    latch.arrive_and_wait();
    for (int i{0}; i < n_ops; ++i) {
      std::lock_guard guard{mutex};
      ++count;
    }
  }};
  {
    latch.arrive_and_wait();
    for (int i{0}; i < n_ops; ++i) {
      std::lock_guard guard{mutex};
      --count;
    }
  }
  thread.join();
  assert(count == 0);
}
