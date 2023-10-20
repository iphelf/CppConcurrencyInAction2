//
// Created by iphelf on 2023-10-20.
//

#include <atomic>
#include <cassert>
#include <latch>
#include <thread>

int main() {
  bool success{true};
  int cnt{1'000};
  do {
    std::atomic<bool> x{false};
    std::atomic<bool> y{false};
    std::atomic<int> z{0};
    std::latch latch{2};
    auto set_x_then_y{[&] {
      latch.arrive_and_wait();
      x.store(true, std::memory_order_relaxed);
      std::atomic_thread_fence(std::memory_order_release);
      y.store(true, std::memory_order_relaxed);
    }};
    auto inc_z_if_y_implies_x{[&] {
      latch.arrive_and_wait();
      while (!y.load(std::memory_order_relaxed))
        ;
      std::atomic_thread_fence(std::memory_order_acquire);
      if (x.load(std::memory_order_relaxed)) ++z;
    }};
    std::thread b{inc_z_if_y_implies_x};
    std::thread a{set_x_then_y};
    a.join();
    b.join();
    success = success && z > 0;
  } while (success && cnt-- > 0);
  assert(success);
}
