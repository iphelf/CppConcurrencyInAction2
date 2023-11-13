//
// Created by iphelf on 2023-11-07.
//

#include <cassert>
#include <functional>
#include <iostream>
#include <latch>
#include <list>
#include <map>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <thread>
#include <vector>

template <typename F, typename T>
concept HashFor = std::regular_invocable<F, T> && requires(F f, T t) {
  { std::invoke(f, t) } -> std::convertible_to<std::size_t>;
};

template <typename Key, typename Value, typename Hash = std::hash<Key>>
  requires HashFor<Hash, Key>
class threadsafe_lookup_table {
  struct Bucket {
    using Entry = std::pair<Key, Value>;
    std::list<Entry> data;
    mutable std::shared_mutex mutex;
    auto get_entry(const Key &key) const {
      auto found{std::find_if(
          data.begin(), data.end(),
          [&key](const Entry &entry) { return entry.first == key; })};
      return found;
    }
    auto get_entry(const Key &key) {
      auto found{std::find_if(
          data.begin(), data.end(),
          [&key](const Entry &entry) { return entry.first == key; })};
      return found;
    }
    void remove_entry(const Key &key) {
      data.remove_if([&key](const Entry &entry) { return entry.first == key; });
    }
  };
  std::vector<Bucket> buckets;
  Hash hasher;

  [[nodiscard]] const Bucket &get_bucket(const Key &key) const {
    std::size_t index{hasher(key) % buckets.size()};
    return buckets[index];
  }
  Bucket &get_bucket(const Key &key) {
    std::size_t index{hasher(key) % buckets.size()};
    return buckets[index];
  }

 public:
  explicit threadsafe_lookup_table(std::size_t capacity) : buckets(capacity) {}
  bool try_get(const Key &key, Value &value) const {
    const Bucket &bucket{get_bucket(key)};
    std::shared_lock lock{bucket.mutex};
    auto entry{bucket.get_entry(key)};
    if (entry == bucket.data.end()) return false;
    value = entry->second;
    return true;
  }
  void set(const Key &key, const Value &value) {
    Bucket &bucket{get_bucket(key)};
    std::unique_lock lock{bucket.mutex};
    auto entry{bucket.get_entry(key)};
    if (entry == bucket.data.end())
      bucket.data.emplace_back(key, value);
    else
      entry->second = value;
  }
  void erase(const Key &key) {
    Bucket &bucket{get_bucket(key)};
    std::unique_lock lock{bucket.mutex};
    bucket.remove_entry(key);
  }
  [[nodiscard]] std::map<Key, Value> snapshot() const {
    std::vector<std::shared_lock<std::shared_mutex>> locks(buckets.size());
    for (int i{0}; i < buckets.size(); ++i)
      locks[i] = std::shared_lock{buckets[i].mutex};
    std::map<Key, Value> result;
    for (auto &bucket : buckets)
      for (auto &p : bucket.data) result.insert(p);
    return result;
  }
};

int main() {
  std::atomic<long long> expected_sum{0LL};
  const int n_threads{static_cast<int>(std::thread::hardware_concurrency())};
  const int n_items_per_thread{1'000};
  const int n_items{n_threads * n_items_per_thread};
  threadsafe_lookup_table<int, int> table{n_items_per_thread};
  {
    std::latch latch{n_threads};
    std::vector<std::thread> threads;
    for (int i_thread{0}; i_thread < n_threads; ++i_thread)
      threads.emplace_back(
          [i_thread, n_items_per_thread, &expected_sum, &table, &latch] {
            std::default_random_engine engine{std::random_device{}()};
            std::uniform_int_distribution distribution;
            latch.arrive_and_wait();
            for (int i{0}; i < n_items_per_thread; ++i) {
              int key{i_thread * n_items_per_thread + i};
              int value{distribution(engine)};
              expected_sum += value;
              table.set(key, value);
            }
          });
    for (auto &thread : threads) thread.join();
  }
  {
    long long actual_sum{0LL};
    auto map{table.snapshot()};
    for (const auto &p : map) actual_sum += p.second;
    assert(actual_sum == expected_sum);
  }
  {
    long long actual_sum{0LL};
    for (int key{0}, value; key < n_items; ++key)
      if (table.try_get(key, value)) actual_sum += value;
    assert(actual_sum == expected_sum);
  }
  {
    long long sum{0LL};
    for (int key{0}, value; key < n_items; ++key) {
      if (key % 2 == 0) table.erase(key);
      table.set(key, key);
      if (table.try_get(key, value)) sum += value;
    }
    assert(sum == n_items * (n_items - 1) / 2);
  }
}
