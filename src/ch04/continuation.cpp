//
// Created by iphelf on 2023-10-01.
//

#include <cassert>
#include <exec/single_thread_context.hpp>
#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

int main() {
  {
    const int a = 1;
    const int b = 2;
    const int a_plus_b = a + b;
    stdexec::sender auto sender_a{stdexec::just(a)};
    stdexec::sender auto sender_a_plus_b{
        stdexec::then(sender_a, [&b](int a) { return a + b; })};
    auto [result]{stdexec::this_thread::sync_wait(sender_a_plus_b).value()};
    assert(result == a_plus_b);
  }
  {
    class an_error : std::exception {};
    exec::single_thread_context context;
    stdexec::scheduler auto scheduler{context.get_scheduler()};
    stdexec::sender auto sender{stdexec::schedule(scheduler)};
    auto lazy{stdexec::then(sender, [] { throw an_error{}; })};
    try {
      auto result{stdexec::this_thread::sync_wait(lazy)};
      assert(result.has_value());
      std::abort();
    } catch (an_error &) {
    }
  }
}