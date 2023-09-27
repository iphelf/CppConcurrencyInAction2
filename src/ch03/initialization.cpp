//
// Created by iphelf on 2023-09-27.
//

#include <cassert>
#include <mutex>
#include <vector>

class DuplicateInitialization : std::exception {
 public:
  DuplicateInitialization() : std::exception("Duplicate Initialization") {}
};

class Resource {
  bool initialized{false};
  std::mutex access_mutex;
  std::once_flag initialized_flag{};
  void initialize() {
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
    if (initialized) throw DuplicateInitialization{};
    initialized = true;
  }

 public:
  void unsafe_use() {
    if (!initialized) initialize();
  }
  void over_safe_use() {
    std::lock_guard lock{access_mutex};
    if (!initialized) initialize();
  }
  void proper_use() {
    std::call_once(initialized_flag, &Resource::initialize, this);
  }
  using use_t = decltype(&Resource::unsafe_use);
};

void test_initialization(Resource::use_t use, bool expect_failure) {
  const int n_tests{5};
  const int n_threads{
      std::max(static_cast<int>(std::thread::hardware_concurrency()), 2)};
  for (int i_test{0}; i_test < n_tests; ++i_test) {
    Resource resource;
    std::vector<std::thread> threads;
    bool failed{false};
    for (int i_thread{0}; i_thread < n_threads; ++i_thread)
      threads.emplace_back([&resource, &failed, use] {
        try {
          std::invoke(use, resource);
        } catch (DuplicateInitialization &) {
          failed = true;
        }
      });
    for (auto &thread : threads) thread.join();
    assert(failed == expect_failure);
  }
}

int main() {
  test_initialization(&Resource::unsafe_use, true);
  test_initialization(&Resource::over_safe_use, false);
  test_initialization(&Resource::proper_use, false);
}