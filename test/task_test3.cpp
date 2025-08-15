//
// Created by lhy on 24-9-19.
//
#include "web/coroutine/co.h"
Task<void> void_func() {
  debug(), "void_func";
  co_return;
}
Task<std::string> baby() {
  co_await void_func();
  debug(), "baby";
  co_return "aaa";
}

Task<double> world() {
  debug(), "world";
  co_return 3.14;
}

Task<int> hello() {
  auto ret = co_await baby();
  debug(), ret;
  int i = co_await world();
  debug(), "hello得到world结果为", i;
  co_return i + 1;
}

int main() {
  debug(), "main即将调用hello";
  auto t = hello();
  debug(), "main调用完了hello";  // 其实只创建了task对象，并没有真正开始执行
  while (t) {
    t();
    debug(), "main得到hello结果为",
        t.get_or_throw();
  }
  return 0;
}