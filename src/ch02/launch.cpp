//
// Created by iphelf on 2023-09-22.
//

#include <cassert>
#include <exception>
#include <thread>
#include <vector>

int main() {
  {
    static int last_id{-1};
    static std::vector<int> records;
    struct Callable {
      int id;
      Callable() : id{++last_id} { records.push_back(id); }
      Callable(const Callable &) : Callable() {}
      Callable(Callable &&rhs) noexcept : id{rhs.id} { rhs.id = -1; }
      ~Callable() {
        if (id != -1) records.push_back(-id);
      }
      void operator()() {}
    };
    {
      last_id = -1;
      records.clear();
      Callable callable;
      std::thread{callable}.join();
    }
    assert(records == (std::vector{0, 1, -1, 0}));
    {
      last_id = -1;
      records.clear();
      std::thread thread{Callable()};
      thread.join();
      assert(!thread.joinable());
    }
    assert(records == (std::vector{0, 0}));
  }

  {
    int n_threads{0};
    {
      std::thread thread{[&n_threads] {
        ++n_threads;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        --n_threads;
      }};
      thread.detach();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    assert(n_threads == 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    assert(n_threads == 0);
  }

  {
    static int n_resource{0};
    struct Resource {
      Resource() { ++n_resource; }
      ~Resource() { --n_resource; }
    };
    std::thread{[] {
      std::thread thread{[] {
        Resource resource;
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
      }};
      // without detach: would be terminated; resource not released
      thread.detach();  // with detach: would be background;
                        // silent finish -> released;
                        // silent termination -> not released.
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      assert(n_resource == 1);
    }}.join();
    assert(n_resource == 1);
    atexit([] { assert(n_resource == 1); });
  }
}