//
// Created by iphelf on 2023-10-06.
//

#include <cassert>
#include <latch>
#include <thread>
#include <vector>
#include <atomic>

int main() {
  const int n_threads{
      std::max(2, static_cast<int>(std::thread::hardware_concurrency()))};
  std::atomic<int> countdown{0};
  {
    std::latch latch{n_threads};
    std::vector<std::thread> threads;
    for (int i{0}; i < n_threads; ++i)
      threads.emplace_back([&] {
        ++countdown;
        latch.count_down();
      });
    latch.wait();
    assert(countdown == n_threads);
    for (auto &thread : threads) thread.join();
  }
  {
    std::latch latch{n_threads};
    std::vector<std::thread> threads;
    for (int i{0}; i < n_threads; ++i)
      threads.emplace_back([&] {
        --countdown;
        latch.count_down();
      });
    latch.wait();
    assert(countdown == 0);
    for (auto &thread : threads) thread.join();
  }
}
