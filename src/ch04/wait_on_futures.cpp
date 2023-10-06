//
// Created by iphelf on 2023-10-03.
//

#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

int main() {
  exec::static_thread_pool thread_pool;
  stdexec::scheduler auto scheduler{thread_pool.get_scheduler()};
  stdexec::sender auto f1{
      stdexec::then(stdexec::schedule(scheduler), [] { return 1; })};
  stdexec::sender auto f2{
      stdexec::then(stdexec::schedule(scheduler), [] { return 2; })};
  stdexec::sender auto f3{
      stdexec::then(stdexec::schedule(scheduler), [] { return 3; })};
  auto fs{stdexec::when_all(f1, f2, f3)};
  auto [r1, r2, r3]{stdexec::sync_wait(fs).value()};
  assert(r1 == 1);
  assert(r2 == 2);
  assert(r3 == 3);
}