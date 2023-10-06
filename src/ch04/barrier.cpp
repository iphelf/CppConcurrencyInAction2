//
// Created by iphelf on 2023-10-06.
//

#include <atomic>
#include <barrier>
#include <cassert>
#include <thread>
#include <vector>

int main() {
  const int n_threads{
      std::max(2, static_cast<int>(std::thread::hardware_concurrency()))};
  std::vector<std::thread> threads;

  {
    std::atomic<int> countdown{0};
    std::barrier barrier{n_threads};
    for (int i{1}; i < n_threads; ++i)
      threads.emplace_back([&] {
        ++countdown;
        barrier.arrive_and_wait();
        barrier.arrive_and_wait();
        --countdown;
        barrier.arrive_and_drop();
      });
    barrier.arrive_and_wait();
    assert(countdown == n_threads - 1);
    barrier.arrive_and_wait();
    barrier.arrive_and_wait();
    assert(countdown == 0);
    for (auto &thread : threads) thread.join();
    threads.clear();
  }

  {
    std::atomic<int> count{0};
    std::barrier barrier(n_threads,
                         [&]() noexcept { assert(count % n_threads == 0); });
    for (int i{0}; i < n_threads; ++i)
      threads.emplace_back([&] {
        for (int i{0}; i < 10; ++i) {
          ++count;
          barrier.arrive_and_wait();
        }
      });
    for (auto &thread : threads) thread.join();
    threads.clear();
  }
}