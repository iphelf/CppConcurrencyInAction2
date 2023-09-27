//
// Created by iphelf on 2023-09-24.
//

#include <cassert>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

struct Resource {
  std::mutex mutex;
  bool locked{false};
  int id{};
  explicit Resource(int id) : id{id} {}
  Resource(Resource &&rhs) noexcept : id{rhs.id} { rhs.id = -1; }
  Resource &operator=(Resource &&rhs) noexcept {
    id = rhs.id;
    rhs.id = -1;
    return *this;
  }
};

auto swaps_performer(bool expect_deadlock,
                     std::function<bool(Resource &, Resource &)> &&swap) {
  return [=](int n_threads, int n_swaps) {
    Resource r1{1}, r2{2};
    std::vector<std::thread> threads;
    const auto start_time{std::chrono::system_clock::now() +
                          std::chrono::milliseconds(100)};
    bool deadlocked{false};
    for (int i{0}; i < n_threads; ++i)
      threads.emplace_back([&] {
        std::this_thread::sleep_until(start_time);
        for (int j{0}; !deadlocked && j < n_swaps; ++j) {
          if (j % 2) {
            if (!swap(r1, r2)) {
              deadlocked = true;
              break;
            }
          } else {
            if (!swap(r2, r1)) {
              deadlocked = true;
              break;
            }
          }
        }
      });
    for (auto &thread : threads) thread.join();
    assert(!(expect_deadlock ^ deadlocked));
  };
}

bool unsafe_swap(Resource &lhs, Resource &rhs) {
  std::lock_guard guard_lhs{lhs.mutex};
  lhs.locked = true;
  if (rhs.locked) return false;
  std::lock_guard guard_rhs{rhs.mutex};
  rhs.locked = true;
  std::swap(lhs, rhs);
  rhs.locked = false;
  lhs.locked = false;
  return true;
}

void unsafe_swaps_almost_certainly_cause_deadlock(int n_threads, int n_swaps) {
  swaps_performer(true, unsafe_swap)(n_threads, n_swaps);
}

bool safe_swap(Resource &lhs, Resource &rhs) {
#if _HAS_CXX17
  std::scoped_lock guard{lhs.mutex, rhs.mutex};
#else
  std::lock(lhs.mutex, rhs.mutex);
  std::lock_guard guard_lhs{lhs.mutex, std::adopt_lock};
  std::lock_guard guard_rhs{rhs.mutex, std::adopt_lock};
#endif
  int deadlocked{lhs.locked ^ rhs.locked};
  if (deadlocked) {
    return false;
  }
  lhs.locked = true;
  rhs.locked = true;
  std::swap(lhs, rhs);
  rhs.locked = false;
  lhs.locked = false;
  return true;
}

void safe_swaps_do_not_cause_deadlock(int n_threads, int n_swaps) {
  swaps_performer(false, safe_swap)(n_threads, n_swaps);
}

int main() {
  const int n_threads{
      std::max(static_cast<int>(std::thread::hardware_concurrency()), 2)};
  const int n_swaps{10'000};
  unsafe_swaps_almost_certainly_cause_deadlock(n_threads, n_swaps);
  safe_swaps_do_not_cause_deadlock(n_threads, n_swaps);
}
