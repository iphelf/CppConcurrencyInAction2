//
// Created by iphelf on 2023-09-30.
//

#include <algorithm>
#include <cassert>
#include <future>
#include <list>
#include <random>

template <typename T>
std::list<T> sort(std::list<T> list) {
  if (list.empty()) return list;
  std::list<T> sorted;
  sorted.splice(sorted.begin(), list, list.begin());
  const T& pivot{sorted.front()};
  auto it{std::partition(list.begin(), list.end(),
                         [&pivot](const T& v) { return v < pivot; })};
  std::list<T> lower;
  lower.splice(lower.begin(), list, list.begin(), it);
  std::future<std::list<T>> new_lower{std::async(sort<T>, std::move(lower))};
  std::list<T> new_higher{sort(std::move(list))};
  sorted.splice(sorted.end(), new_higher);
  sorted.splice(sorted.begin(), new_lower.get());
  return sorted;
}

int main() {
  const int n{1000};
  std::random_device random_device{};
  std::default_random_engine engine{random_device()};
  std::uniform_int_distribution dist{0, n};
  std::list<int> list;
  for (int i{0}; i < n; ++i) list.push_back(dist(engine));

  std::list<int> sorted{list};
  sorted.sort();

  std::list<int> p_sorted{sort(list)};

  assert(sorted == p_sorted);
}
