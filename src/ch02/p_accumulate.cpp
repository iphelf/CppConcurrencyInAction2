//
// Created by iphelf on 2023-09-22.
//

#include <algorithm>
#include <cassert>
#include <mutex>
#include <numeric>
#include <thread>
#include <vector>

constexpr bool benchmark{true};

std::vector<std::pair<std::thread::id, std::size_t>> records;
std::mutex records_mutex;
std::size_t threshold{10'000'000};

template <typename Iterator, typename T>
T p_accumulate(Iterator first, Iterator last, T init) {
  auto distance{std::distance(first, last)};
  if (distance <= threshold) {
    if constexpr (!benchmark) {
      std::scoped_lock lock{records_mutex};
      records.emplace_back(std::this_thread::get_id(), distance);
    }
    return std::accumulate(first, last, init);
  }
  auto mid{first + distance / 2};
  T sum1{init};
  std::thread thread{[&] { sum1 += p_accumulate(first, mid, init); }};
  T sum2{p_accumulate(mid, last, init) - init};
  thread.join();
  return sum1 + sum2;
}

int main() {
  std::vector<long long> v(200'000'000);
  std::iota(v.begin(), v.end(), 1);
  auto tp{std::chrono::system_clock::now()};
  auto sum{std::accumulate(v.begin(), v.end(), 0LL)};
  auto tp_s{std::chrono::system_clock::now()};
  auto p_sum{p_accumulate(v.begin(), v.end(), 0LL)};
  auto tp_p{std::chrono::system_clock::now()};
  assert(sum == p_sum);
  auto d_s{tp_s - tp};
  auto d_p{tp_p - tp_s};
  if constexpr (benchmark) {
    assert(d_s > d_p * 2);
  } else {
    assert(std::all_of(records.begin(), records.end(),
                       [](const std::pair<std::thread::id, std::size_t> &p) {
                         return p.second <= threshold;
                       }));
  }
}