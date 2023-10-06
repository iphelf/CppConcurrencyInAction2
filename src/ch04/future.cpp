//
// Created by iphelf on 2023-09-30.
//

#include <cassert>
#include <future>
#include <queue>
#include <thread>

void test_async() {
  auto this_thread_id{std::this_thread::get_id()};

  {
    std::future<std::thread::id> thread_id = std::async(
        std::launch::async, [] { return std::this_thread::get_id(); });
    assert(!(thread_id.get() == this_thread_id));
  }

  {
    std::future<std::thread::id> thread_id = std::async(
        std::launch::deferred, [] { return std::this_thread::get_id(); });
    assert(thread_id.get() == this_thread_id);
  }

  {
    std::future<std::thread::id> thread_id =
        std::async([] { return std::this_thread::get_id(); });
    assert(!(thread_id.get() == this_thread_id));
  }

  {
    bool block_on_destruct{false};
    {
      auto future{std::async([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
        block_on_destruct = true;
      })};
    }
    assert(block_on_destruct);
  }
}

void test_packaged_task() {
  std::atomic<bool> stop{false};
  std::mutex mutex;
  std::condition_variable cond;
  std::queue<std::packaged_task<std::thread::id()>> queue;
  std::thread background_thread{[&] {
    while (!stop) {
      std::unique_lock lock{mutex};
      cond.wait(lock, [&] { return !queue.empty(); });
      auto task{std::move(queue.front())};
      queue.pop();
      lock.unlock();
      task();
    }
  }};

  std::thread::id this_thread_id{std::this_thread::get_id()};
  auto get_background_thread_id{[&] {
    std::packaged_task<std::thread::id()> task{
        [] { return std::this_thread::get_id(); }};
    auto thread_id_future{task.get_future()};
    {
      std::lock_guard lock{mutex};
      queue.push(std::move(task));
      cond.notify_one();
    }
    return thread_id_future.get();
  }};

  std::thread::id background_thread_id = get_background_thread_id();
  assert(!(this_thread_id == background_thread_id));

  {
    auto thread_id{get_background_thread_id()};
    assert(thread_id == background_thread_id);
  }

  {
    std::lock_guard lock{mutex};
    queue.emplace([&] {
      stop = true;
      return std::this_thread::get_id();
    });
    cond.notify_one();
  }
  background_thread.join();
}

void test_promise() {
  std::promise<std::thread::id> promise;
  std::future<std::thread::id> future = promise.get_future();
  std::thread thread{[&promise] {
    std::this_thread::sleep_for(std::chrono::milliseconds{10});
    promise.set_value(std::this_thread::get_id());
  }};
  std::thread::id another_thread_id{future.get()};
  assert(!(another_thread_id == std::this_thread::get_id()));
  thread.join();
}

void test_exception() {
  class my_exception : std::exception {};

  {
    auto future{std::async([] { throw my_exception{}; })};
    try {
      future.get();
      std::abort();
    } catch (my_exception &) {
    }
  }

#ifndef __clang__
  {
    std::future<void> future;
    {
      std::packaged_task task{[] {}};
      future = task.get_future();
    }
    try {
      future.get();
      std::abort();
    } catch (std::future_error &e) {
      assert(e.code() == std::future_errc::broken_promise);
    }
  }
#endif

  {
    std::promise<void> promise;
    std::future<void> future = promise.get_future();
    promise.set_exception(std::make_exception_ptr(my_exception{}));
    try {
      future.get();
      std::abort();
    } catch (my_exception &) {
    }
  }
}

void test_shared_future() {
  const int n_threads{
      std::max(2, static_cast<int>(std::thread::hardware_concurrency())) - 1};
  std::promise<void> promise;
  std::shared_future<void> shared_future = promise.get_future().share();
  std::vector<std::thread> threads;
  std::atomic<int> n_waiting{0};
  for (int i_thread{0}; i_thread < n_threads; ++i_thread)
    threads.emplace_back([shared_future, &n_waiting] {
      ++n_waiting;
      shared_future.wait();
      --n_waiting;
    });
  std::this_thread::sleep_for(std::chrono::milliseconds{10});
  assert(n_waiting == n_threads);
  promise.set_value();
  std::this_thread::sleep_for(std::chrono::milliseconds{10});
  assert(n_waiting == 0);
  for (auto &thread : threads) thread.join();
}

int main() {
  test_async();
  test_packaged_task();
  test_promise();
  test_exception();
  test_shared_future();
}
