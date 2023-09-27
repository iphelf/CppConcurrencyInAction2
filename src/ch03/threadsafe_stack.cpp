//
// Created by iphelf on 2023-09-24.
//

#include <cassert>
// #include <iostream>
#include <memory>
#include <mutex>
#include <stack>
#include <thread>

template <typename T>
class threadsafe_stack {
  std::mutex mutex{};
  std::stack<T> data{};
  int n_log{0};
  void log() {
    // std::cout << '#' << n_log << ": " << data.size() << '\n';
    ++n_log;
  }

 public:
  threadsafe_stack() = default;
  threadsafe_stack(const threadsafe_stack &rhs) {
    std::lock_guard guard{mutex};  // necessary?
    data = rhs.data;
  }
  threadsafe_stack &operator=(const threadsafe_stack &) = delete;
  threadsafe_stack(threadsafe_stack &&) = delete;
  threadsafe_stack &operator=(threadsafe_stack &&) = delete;
  std::shared_ptr<T> pop() {
    std::lock_guard guard{mutex};
    if (data.empty()) return nullptr;
    auto popped{std::make_shared<T>(data.top())};
    data.pop();
    log();
    return popped;
  }
  bool pop(T &recipient) {
    std::lock_guard guard{mutex};
    if (data.empty()) return false;
    recipient = data.top();
    data.pop();
    log();
    return true;
  }
  void push(T item) {
    std::lock_guard guard{mutex};
    data.push(std::move(item));
    log();
  }
};

int main() {
  const int n_items{100'000};
  const int n_threads{
      std::max(static_cast<int>(std::thread::hardware_concurrency()), 2)};
  threadsafe_stack<int> stack;
  std::stack<std::thread> threads;
  const auto start_time{std::chrono::system_clock::now() +
                        std::chrono::milliseconds(100)};
  std::atomic<long long> content;
  for (int i{0}; i < n_threads; ++i)
    threads.emplace([=, &stack, &content] {
      std::this_thread::sleep_until(start_time);
      if (i % 2) {
        for (int j{0}; j < n_items; ++j) {
          stack.push(j);
          content += j;
        }
        for (int j{0}, result; j < n_items; ++j) {
          while (!stack.pop(result)) std::this_thread::yield();
          content -= result;
        }
      } else {
        for (int j{0}; j < n_items; ++j) {
          stack.push(j);
          content += j;
          std::shared_ptr<int> result;
          while (true) {
            result = stack.pop();
            if (result) break;
            std::this_thread::yield();
          }
          content -= *result;
        }
      }
    });
  while (!threads.empty()) {
    std::thread thread{std::move(threads.top())};
    threads.pop();
    thread.join();
  }
  assert(threads.empty());
  assert(!stack.pop());
  assert(content == 0);
}
