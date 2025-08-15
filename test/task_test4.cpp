//
// Created by lhy on 24-9-19.
//
#include <chrono>

#include "web/coroutine/co.h"
using std::chrono_literals::operator""s;
auto st_time = std::chrono::steady_clock::now();
Task<int> hello1() {
  debug(), "hello1开始睡5秒";
  co_await 5s;
  debug(), "hello1睡醒了", (std::chrono::steady_clock::now() - st_time) / 1s;
  co_return 1;
}

Task<int> hello2() {
  debug(), "hello2开始睡6秒";
  co_await 6s;
  debug(), "hello2睡醒了", (std::chrono::steady_clock::now() - st_time) / 1s;
  co_return 2;
}
Task<int> hello() {
  auto ret = co_await hello1();
  ret = co_await hello2()+ret;
  co_return std::move(ret);
}
int main() {
  auto t1 = hello1();
  auto t2 = hello2();
  auto t = hello();
  t1();
  t2();
  t();
  debug(), "主函数中得到hello1结果:", t1.get_or_throw();
  debug(), "主函数中得到hello2结果:", t2.get_or_throw();
  debug(), "主函数中得到hello结果:", t.get_or_throw();
  return 0;
}