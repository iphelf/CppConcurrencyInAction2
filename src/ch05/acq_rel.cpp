//
// Created by iphelf on 2023-10-18.
//

#include <atomic>
#include <cassert>
#include <latch>
#include <thread>

void test_synchronization() {
  std::atomic<bool> x{false};
  std::atomic<bool> y{false};
  std::latch latch{2};
  std::thread write_x_then_y{[&] {
    latch.arrive_and_wait();
    x.store(true, std::memory_order_relaxed);
    y.store(true, std::memory_order_release);
  }};
  std::thread read_y_then_x{[&] {
    latch.arrive_and_wait();
    while (!y.load(std::memory_order_acquire))
      ;
    bool x_loaded{x.load(std::memory_order_relaxed)};
    assert(x_loaded);
  }};
  write_x_then_y.join();
  read_y_then_x.join();
}

void test_transitivity() {
  const int n{5};
  std::atomic<int> data[n];
  for (int i{0}; i < n; ++i) data[i] = i;
  std::atomic<int> state{0};

  std::latch latch{3};
  std::thread write_data{[&] {
    latch.arrive_and_wait();
    for (int i{n - 1}; i >= 0; --i)
      data[i].store(n - 1 - i, std::memory_order_relaxed);
    state.store(1, std::memory_order_release);
  }};
  std::thread trans{[&] {
    latch.arrive_and_wait();
    while (state.load(std::memory_order_acquire) != 1)
      ;
    state.store(2, std::memory_order_release);
  }};
  std::thread read_data{[&] {
    latch.arrive_and_wait();
    while (state.load(std::memory_order_acquire) != 2)
      ;
    for (int i{0}; i < n; ++i) {
      int loaded{data[i].load(std::memory_order_relaxed)};
      assert(loaded == n - 1 - i);
    }
  }};
  write_data.join();
  trans.join();
  read_data.join();
}

int main() {
  const int n{1'000};
  for (int i{0}; i < n; ++i) {
    test_synchronization();
    test_transitivity();
  }
}
