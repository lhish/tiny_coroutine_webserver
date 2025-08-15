#include <sys/epoll.h>
#include <sys/ioctl.h>

#include <string>

#include "web/coroutine/co.h"

using namespace std::chrono_literals;
Task<std::string> reader() {
  auto which = co_await when_any(wait_file(0, EPOLLIN), sleep_for(1s));
  if (which.index() != 0) {
    co_return "超过1秒没有收到任何输入";
  }
  std::string s;
  while (true) {
    char c;
    ssize_t len = read(0, &c, 1);
    if (len == -1) {
      if (errno != EWOULDBLOCK) [[unlikely]] {
        throw std::system_error(errno, std::system_category());
      }
      break;
    }
    s.push_back(c);
  }
  co_return std::move(s);
}

lhy::Task<int> async_main() {
  while (true) {
    auto s = co_await reader();
    lhy::debug(), "读到了", s;
    if (s == "quit\n") break;
  }
  co_return 1;
}

int main() {
  int attr = 1;
  lhy::debug(), ioctl(STDIN_FILENO, FIONBIO, &attr);

  auto t = async_main();
  t.handle_.resume();
  while (!t.handle_.done()) {
    if (auto delay = lhy::GetLoop().TryRun()) {
      auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(*delay).count();
      lhy::GetEpollLoop().TryRun(ms);
    } else {
      lhy::GetEpollLoop().TryRun(-1);
    }
  }

  return 0;
}