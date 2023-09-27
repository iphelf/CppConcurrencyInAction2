//
// Created by iphelf on 2023-09-27.
//

#include <cassert>
#include <mutex>
#include <thread>

int main() {
  std::recursive_mutex recursive_mutex;
  {
    std::lock_guard l1{recursive_mutex};
    std::lock_guard l2{recursive_mutex};
    std::lock_guard l3{recursive_mutex};
  }
  {
    std::lock_guard main{recursive_mutex};
    std::thread{[&] {
      std::unique_lock secondary{recursive_mutex, std::defer_lock};
      assert(!secondary.try_lock());
    }}.join();
  }
}