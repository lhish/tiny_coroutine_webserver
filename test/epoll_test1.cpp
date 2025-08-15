//
// Created by lhy on 24-9-26.
//
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cerrno>

#include "web/coroutine/co.h"

int main() {
  int attr = 1;
  /// 句柄，非阻塞标志，0阻塞，1非阻塞
  ioctl(0, FIONBIO, &attr);
  /// 创建epoll句柄
  int epfd = epoll_create1(0);
  /// 这个监听事件只是为了设置监听，不关注事件
  epoll_event event{};
  /// 设置监听事件
  event.events = EPOLLIN;
  /// 设置监听文件描述符
  event.data.fd = 0;
  /// 添加监听事件,ADD就是一个操作
  epoll_ctl(epfd, EPOLL_CTL_ADD, 0, &event);
  while (true) {
    epoll_event read_event[10];
    // 等待事件发生，阻塞，如果不加的话，就会使CPU占用很高，能占满一个核
    /// 感觉一个event 只对应一个文件描述符，所以一个文件描述符对应一个event
    const auto res = epoll_wait(epfd, read_event, 10, 1000);
    if (res < 0) {
      printf("epoll_wait error\n");
      return -1;
    }
    if (res == 0) {
      printf("timeout\n");
      continue;
    }
    for (int i = 0; i < res; ++i) {
      /// 获取事件
      /// events记录事件类型，data记录文件描述符
      auto& [events, data] = read_event[i];
      /// 获取文件描述符
      auto fd = data.fd;
      char c;
      int count = 0;
      while (true) {
        count++;
        /// 读入数据
        if (read(fd, &c, 1) <= 0) {
          /// 非阻塞
          if(errno == EWOULDBLOCK) {
            break;
          }
        }
        debug(),c;
      }
      debug(),"count:",count;
    }
  }
}