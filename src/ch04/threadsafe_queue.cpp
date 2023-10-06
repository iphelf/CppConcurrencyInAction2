//
// Created by iphelf on 2023-09-29.
//

#include <algorithm>
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

const bool dump_records{false};

template <typename T>
class threadsafe_queue {
  std::queue<T> data{};
  mutable std::mutex mutex{};
  std::condition_variable cond{};

 public:
  threadsafe_queue() = default;
  threadsafe_queue(const threadsafe_queue &rhs) {
    std::lock_guard lock{rhs.mutex};
    data = rhs.data;
  }
  threadsafe_queue &operator=(const threadsafe_queue &) = delete;
  threadsafe_queue(threadsafe_queue &&) = delete;
  threadsafe_queue &operator=(threadsafe_queue &&) = delete;
  bool empty() const {
    std::lock_guard lock{mutex};
    return data.empty();
  }
  void push(T item) {
    {
      std::lock_guard lock{mutex};
      data.push(std::move(item));
    }
    cond.notify_one();
  }
  bool try_pop(T &recipient) {
    std::lock_guard lock{mutex};
    if (data.empty()) return false;
    recipient = std::move(data.front());
    data.pop();
    return true;
  }
  std::unique_ptr<T> try_pop() {
    std::lock_guard lock{mutex};
    if (data.empty()) return nullptr;
    auto popped{std::make_unique<T>(std::move(data.front()))};
    data.pop();
    return popped;
  }
  void wait_and_pop(T &recipient) {
    std::unique_lock lock{mutex};
    cond.wait(lock, [this] { return !data.empty(); });
    recipient = std::move(data.front());
    data.pop();
  }
  std::unique_ptr<T> wait_and_pop() {
    std::unique_lock lock{mutex};
    cond.wait(lock, [this] { return !data.empty(); });
    auto popped{std::make_unique<T>(std::move(data.front()))};
    data.pop();
    return popped;
  }
};

struct record {
  std::chrono::microseconds b;
  std::chrono::microseconds e;
  int id;
  int i_consumer;
};
std::ostream &operator<<(std::ostream &os, const record &r) {
  return os << "dict(Task=" << r.id << ", Start=" << r.b.count()
            << ", Finish=" << r.e.count() << ", Resource=" << r.i_consumer
            << ')';
}

int main() {
  const int n_threads{
      std::max(2, static_cast<int>(std::thread::hardware_concurrency()))};
  const int n_consumers{n_threads - 1};
  const int n_items{n_consumers * 5};
  const std::chrono::microseconds time_to_produce{10'000};
  const std::chrono::microseconds time_to_consume{time_to_produce *
                                                  n_consumers};
  const int stop_item{-1};
  threadsafe_queue<int> queue;
  std::vector<std::thread> consumers;
  std::vector<std::vector<record>> consumers_records(n_consumers + 1);
  std::atomic<int> n_blocked{0};

  auto time_origin{std::chrono::system_clock::now()};
  auto get_relative_time{[&] {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now() - time_origin);
  }};

  for (int i_consumer{0}; i_consumer < n_consumers; ++i_consumer)
    consumers.emplace_back([&, i_consumer] {
      auto process{[&](int i) {
        auto b{get_relative_time()};
        decltype(b) x;
        std::this_thread::sleep_for(time_to_consume);
        if constexpr (dump_records) {
          consumers_records[i_consumer].push_back(
              {b, get_relative_time(), i, i_consumer});
        }
      }};
      if (i_consumer % 2 == 0) {
        int item;
        while (true) {
          ++n_blocked;
          queue.wait_and_pop(item);
          --n_blocked;
          if (item == stop_item) break;
          process(item);
        }
      } else {
        std::unique_ptr<int> item;
        while (true) {
          ++n_blocked;
          item = queue.wait_and_pop();
          --n_blocked;
          if (*item == stop_item) break;
          process(*item);
        }
      }
    });

  std::this_thread::sleep_for(time_to_produce);
  assert(n_blocked == n_consumers);

  {
    auto &producer_records{consumers_records[n_consumers]};
    for (int i{0}; i < n_items; ++i) {
      auto b{get_relative_time()};
      queue.push(i);
      std::this_thread::sleep_for(time_to_produce);
      if constexpr (dump_records) {
        producer_records.push_back({b, get_relative_time(), i, n_consumers});
      }
    }
  }

  auto producing_end_time{std::chrono::system_clock::now()};
  for (int i{0}; i < n_consumers; ++i) queue.push(stop_item);
  for (auto &consumer : consumers) consumer.join();
  auto offset{std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now() - producing_end_time)};
  assert(offset < time_to_consume);
  assert(queue.empty());

  if constexpr (dump_records) {
    std::cout << "df = pd.DataFrame([\n";
    std::sort(
        consumers_records.begin(), consumers_records.end(),
        [](const auto &a, const auto &b) { return a.front().b < b.front().b; });
    for (int i_consumer{0}; i_consumer < consumers_records.size();
         ++i_consumer) {
      auto &records{consumers_records[i_consumer]};
      for (int i_record{0}; i_record < records.size(); ++i_record) {
        records[i_record].i_consumer = i_consumer;
        std::cout << "    " << records[i_record] << ",\n";
      }
    }
    std::cout << "])\n";
  }
}
