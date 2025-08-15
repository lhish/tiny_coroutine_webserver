//
// Created by lhy on 25-7-29.
//
#ifndef DEBUG_H
#define DEBUG_H
#include <iostream>
#include<thread>
class Debug {
 public:
  template <typename... Args>
  Debug operator,(Args&&... other) {
    std::cout << "thread:" << std::this_thread::get_id() << ":";
    std::cout << (other << ...);
    std::cout << std::endl;
    return *this;
  }
};
inline Debug& debug() {
  static Debug debug_{};
  return debug_;
}
#endif  // DEBUG_H
