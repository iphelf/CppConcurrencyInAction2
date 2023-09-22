//
// Created by iphelf on 2023-09-22.
//

#include <cassert>
#include <thread>

int main() {
  static bool hello{false};
  std::thread{[] { hello = true; }}.join();
  assert(hello);
}
