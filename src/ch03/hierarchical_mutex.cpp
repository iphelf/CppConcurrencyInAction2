//
// Created by iphelf on 2023-09-25.
//

#include <cassert>
#include <mutex>
#include <string>
#include <unordered_set>

class duplicate_hierarchy : public std::exception {
 public:
  explicit duplicate_hierarchy(std::size_t hierarchy)
      : std::exception("duplicate hierarchy"), hierarchy{hierarchy} {}
  const std::size_t hierarchy;
};

class hierarchy_violation : public std::exception {
 public:
  hierarchy_violation(std::size_t this_thread_hierarchy,
                      std::size_t current_hierarchy)
      : std::exception("hierarchy of mutexes is violated"),
        this_thread_hierarchy{this_thread_hierarchy},
        current_hierarchy{current_hierarchy} {}
  const std::size_t this_thread_hierarchy;
  const std::size_t current_hierarchy;
};

class hierarchical_mutex {
 public:
  const std::size_t hierarchy;

 private:
  std::size_t previous_hierarchy{0};
  std::mutex mutex;
  static thread_local std::size_t this_thread_hierarchy;
  static thread_local std::unordered_set<std::size_t>
      this_thread_registered_hierarchies;

 public:
  /// the smaller priority value, the higher priority
  explicit hierarchical_mutex(std::size_t priority) : hierarchy{priority} {
    if (this_thread_registered_hierarchies.count(priority))
      throw duplicate_hierarchy{priority};
    this_thread_registered_hierarchies.insert(priority);
  }
  void lock() {
    if (hierarchy <= this_thread_hierarchy)
      throw hierarchy_violation{this_thread_hierarchy, hierarchy};
    mutex.lock();
    previous_hierarchy = this_thread_hierarchy;
    this_thread_hierarchy = hierarchy;
  }
  void unlock() {
    if (hierarchy != this_thread_hierarchy)
      throw hierarchy_violation{this_thread_hierarchy, hierarchy};
    this_thread_hierarchy = previous_hierarchy;
    mutex.unlock();
  }
};
thread_local std::size_t hierarchical_mutex::this_thread_hierarchy{0};
thread_local std::unordered_set<std::size_t>
    hierarchical_mutex::this_thread_registered_hierarchies{};

int main() {
  hierarchical_mutex high_mutex{1000};
  hierarchical_mutex mid_mutex{2000};
  hierarchical_mutex low_mutex{3000};

  {
    std::lock_guard high_guard{high_mutex};
    std::lock_guard mid_guard{mid_mutex};
    std::lock_guard low_guard{low_mutex};
  }

  try {
    std::lock_guard high_guard{high_mutex};
    std::lock_guard low_guard{low_mutex};
    std::lock_guard mid_guard{mid_mutex};
    std::abort();
  } catch (hierarchy_violation &hv) {
    assert(hv.this_thread_hierarchy == low_mutex.hierarchy);
    assert(hv.current_hierarchy == mid_mutex.hierarchy);
  } catch (std::exception &) {
    throw;
  }

  try {
    hierarchical_mutex mutex{high_mutex.hierarchy};
    std::abort();
  } catch (duplicate_hierarchy &dh) {
    assert(dh.hierarchy == high_mutex.hierarchy);
  } catch (std::exception &) {
    throw;
  }
}