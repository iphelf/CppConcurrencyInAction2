//
// Created by iphelf on 2023-11-02.
//

#include <algorithm>
#include <cassert>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

const bool dump_records{false};

template <typename T>
class threadsafe_linked_queue {
  struct node {
    std::shared_ptr<T> data{nullptr};
    std::unique_ptr<node> next{nullptr};
  };

  std::unique_ptr<node> head{std::make_unique<node>()};
  node *tail{head.get()};
  std::mutex head_mutex;
  std::mutex tail_mutex;
  std::condition_variable cond;

  node *get_tail() {
    std::scoped_lock lock{tail_mutex};
    return tail;
  }
  std::shared_ptr<T> pop_head(std::unique_lock<std::mutex> &&lock) {
    std::unique_ptr<node> popped = std::move(head);
    head = std::move(popped->next);
    return popped->data;
  }

 public:
  void push(std::shared_ptr<T> item) {
    {
      std::scoped_lock lock{tail_mutex};
      tail->data = item;
      tail->next = std::make_unique<node>();
      tail = tail->next.get();
    }
    cond.notify_one();
  }
  std::shared_ptr<T> try_pop() {
    std::scoped_lock lock{head_mutex};
    if (head.get() == get_tail()) return nullptr;
    return pop_head(std::move(lock));
  }
  std::shared_ptr<T> wait_and_pop() {
    std::unique_lock lock{head_mutex};
    cond.wait(lock, [this] { return head.get() != get_tail(); });
    return pop_head(std::move(lock));
  }
  bool empty() {
    std::scoped_lock lock{head_mutex};
    return head.get() == get_tail();
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
  threadsafe_linked_queue<int> queue;
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
      std::shared_ptr<int> item;
      while (true) {
        ++n_blocked;
        item = queue.wait_and_pop();
        --n_blocked;
        if (*item == stop_item) break;
        process(*item);
      }
    });

  std::this_thread::sleep_for(time_to_produce);
  assert(n_blocked == n_consumers);

  {
    auto &producer_records{consumers_records[n_consumers]};
    for (int i{0}; i < n_items; ++i) {
      auto b{get_relative_time()};
      queue.push(std::make_shared<int>(i));
      std::this_thread::sleep_for(time_to_produce);
      if constexpr (dump_records) {
        producer_records.push_back({b, get_relative_time(), i, n_consumers});
      }
    }
  }

  auto producing_end_time{std::chrono::system_clock::now()};
  for (int i{0}; i < n_consumers; ++i)
    queue.push(std::make_shared<int>(stop_item));
  for (auto &consumer : consumers) consumer.join();
  auto offset{std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::system_clock::now() - producing_end_time)};

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

  assert(queue.empty());
}
