//
// Created by lhy on 24-9-19.
//
#include <numeric>

#include "web/coroutine/co.h"
using namespace std::chrono_literals;
Task<int> hello1() {
  debug(), "hello1开始睡1秒";
  co_await 1s;  // 1s 等价于 std::chrono::seconds(1)
  debug(), "hello1睡醒了";
  co_return 1;
}

Task<int> hello2() {
  debug(), "hello2开始睡2秒";
  co_await 2s;  // 2s 等价于 std::chrono::seconds(2)
  debug(), "hello2睡醒了";
  co_return 2;
}

// Task<int> hello() {
  // debug(), "hello开始等1和2";
  // auto v = co_await when_any(hello1(), hello2(), hello2());
  // debug(), "hello看到", (int)v.index() + 1, "睡醒了";
  // co_return std::move(std::get<0>(v));
// }
Task<int> hello_() {
  debug(), "hello开始等1和2";

  auto v = co_await WhenAll(hello1(), hello2(), hello2());
  std::apply([](auto&&... args) { debug(), (args, ...); }, v);
  co_return std::apply([](auto&&... args) { return 0 + (args + ...); }, v);
}

int main() {
  // auto t = hello();
  // GetLoop().AddTask(t);
  // GetLoop().RunAll();
  auto t1 = hello_();
  t1();
  debug(), "主函数中得到hello结果:", t1.get_or_throw();
  return 0;
}