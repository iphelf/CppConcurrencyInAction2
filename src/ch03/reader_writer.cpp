//
// Created by iphelf on 2023-09-27.
//

#include <cassert>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <thread>
#include <vector>

class Resource {
 public:
  virtual ~Resource() = default;
  virtual void read() = 0;
  virtual void write() = 0;
};

class PessimisticResource : public Resource {
  std::mutex mutex;

 public:
  void read() override { std::lock_guard lock{mutex}; }
  void write() override { std::lock_guard lock{mutex}; }
};

class OptimisticResource : public Resource {
  std::shared_mutex shared_mutex;

 public:
  void read() override { std::shared_lock lock{shared_mutex}; }
  void write() override { std::lock_guard lock{shared_mutex}; }
};

std::chrono::milliseconds benchmark_read_write(Resource &resource) {
  std::chrono::time_point start_time{std::chrono::system_clock::now()};

  const int n_threads{
      std::max(2, static_cast<int>(std::thread::hardware_concurrency()))};
  const int n_accesses{100'000};
  const float write_ratio{0.05f};
  std::random_device rd;
  std::default_random_engine engine{rd()};
  std::uniform_real_distribution distribution{0.0f, 1.0f};

  std::vector<std::thread> threads;
  for (int i_thread{0}; i_thread < n_threads; ++i_thread)
    threads.emplace_back([&] {
      for (int i{0}; i < n_accesses; ++i) {
        if (distribution(engine) < write_ratio)
          resource.write();
        else
          resource.read();
      }
    });
  for (auto &thread : threads) thread.join();

  return std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now() - start_time);
}

int main() {
  PessimisticResource pessimistic_resource;
  auto d1{benchmark_read_write(pessimistic_resource)};
  OptimisticResource optimistic_resource;
  auto d2{benchmark_read_write(optimistic_resource)};
  assert(d1 > d2);
}