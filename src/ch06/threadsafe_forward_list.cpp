//
// Created by iphelf on 2023-11-07.
//

#include <cassert>
#include <latch>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

template <typename T>
class threadsafe_forward_list {
  struct Node {
    std::shared_ptr<T> data{nullptr};
    std::unique_ptr<Node> next{nullptr};
    std::mutex mutex;
  };
  // dummy head
  std::unique_ptr<Node> head{std::make_unique<Node>()};

 public:
  void push_front(const T &item) {
    std::unique_ptr<Node> new_node{std::make_unique<Node>()};
    new_node->data = std::make_shared<T>(item);
    std::scoped_lock lock{head->mutex};
    new_node->next = std::move(head->next);
    head->next = std::move(new_node);
  }

  template <typename Visit>
  void for_each(Visit visit) {
    std::unique_lock last_lock{head->mutex};
    Node *last_node{head.get()};
    while (Node * node{last_node->next.get()}) {
      std::unique_lock lock{node->mutex};
      last_lock.unlock();
      visit(*node->data);
      last_node = node;
      last_lock = std::move(lock);
    }
  }

  template <typename Predicate>
  std::shared_ptr<T> find_first_if(Predicate predicate) {
    std::unique_lock last_lock{head->mutex};
    Node *last_node{head.get()};
    while (Node * node{last_node->next.get()}) {
      std::unique_lock lock{node->mutex};
      last_lock.unlock();
      if (predicate(*node->data)) return node->data;
      last_node = node;
      last_lock = std::move(lock);
    }
    return nullptr;
  }

  template <typename Predicate>
  void remove_if(Predicate predicate) {
    std::unique_lock last_lock{head->mutex};
    Node *last_node{head.get()};
    while (Node * node{last_node->next.get()}) {
      std::unique_lock lock{node->mutex};
      if (predicate(*node->data)) {
        std::unique_ptr<Node> deleting_node{std::move(last_node->next)};
        last_node->next = std::move(node->next);
        lock.unlock();
      } else {
        last_lock.unlock();
        last_node = node;
        last_lock = std::move(lock);
      }
    }
  }

  bool empty() {
    std::scoped_lock lock{head->mutex};
    return !head->next;
  }
};

int main() {
  {
    const int n{10};
    threadsafe_forward_list<int> list;
    for (int i{0}; i < n; ++i) list.push_front(i);
    {
      int sum{0};
      list.for_each([&sum](int item) { sum += item; });
      assert(sum == n * (n - 1) / 2);
    }
    for (int i{0}; i < n; ++i) {
      auto found{list.find_first_if([i](int item) { return item == i; })};
      assert(found && *found == i);
    }
    list.remove_if([](int item) { return item % 2 != 0; });
    {
      int sum{0};
      list.for_each([&sum](int item) { sum += item; });
      // 0 2 4 6 8
      assert(sum == 20);
    }
  }

  {
    const int n_threads{static_cast<int>(std::thread::hardware_concurrency())};
    const int n_items_per_thread{1'000};
    threadsafe_forward_list<int> list;
    std::vector<std::thread> threads;
    std::latch latch{n_threads};
    for (int i_thread{0}; i_thread < n_threads; ++i_thread)
      threads.emplace_back([&, i_thread] {
        int item_base{i_thread * n_items_per_thread};
        std::default_random_engine engine{std::random_device{}()};
        std::uniform_int_distribution dist{0, n_items_per_thread - 1};
        latch.arrive_and_wait();
        for (int i{0}; i < n_items_per_thread; ++i) {
          int new_item{item_base + i};
          list.push_front(new_item);
          int existing_item{item_base + dist(engine) % (i + 1)};
          auto found{list.find_first_if(
              [existing_item](int item) { return item == existing_item; })};
          assert(found && *found == existing_item);
        }
        list.remove_if(
            [&](int item) { return item / n_items_per_thread == i_thread; });
      });
    for (auto &thread : threads) thread.join();
    assert(list.empty());
  }
}
