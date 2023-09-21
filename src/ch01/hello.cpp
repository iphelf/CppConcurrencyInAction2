//
// Created by iphelf on 2023-09-22.
//

#include <iostream>
#include <thread>

int main() {
  std::thread{[] { std::cout << "Hello, world!\n"; }}.join();
}
