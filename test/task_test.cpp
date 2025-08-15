#include "web/coroutine/co.h"
Task<int> world() {
  debug(), "world";
  co_yield 422;
  co_yield 444;
  co_yield 444;
  co_return 100;
}

Task<int> hello() {
  debug(), "hello 正在构建worldTask";
  auto world_task = world();
  debug(), "hello 构建完了worldTask，开始等待world";
  auto ret = co_await world_task;
  debug(), "hello得到world返回", ret;
  ret = co_await world_task;
  debug(), "hello得到world返回", ret;
  ret = co_await world_task;
  debug(), "hello得到world返回", ret;
  ret = co_await world_task;
  debug(), "hello得到world返回", ret;
  debug(), "hello 42";
  co_yield 42;
  debug(), "hello 12";
  co_yield 12;
  debug(), "hello 6";
  co_yield 6;
  debug(), "hello 结束";
  co_return 100;
}

int main() {
  debug(), "main即将调用hello";
  Task t = hello();
  debug(), "main调用完了hello";  // 其实只创建了task对象，并没有真正开始执行
  while (t) {
    t();
    debug(), "main得到hello结果为",
        t.get_or_throw();
  }
  return 0;
}
