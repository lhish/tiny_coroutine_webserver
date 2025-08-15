//
// Created by lhy on 24-9-26.
//

#ifndef SOCKET_ADDR_H
#define SOCKET_ADDR_H
#include <netdb.h>

#include <string>

#include "help_func.h"
namespace lhy {
enum class SocketType { TCP, UDP };
class SocketAddr {
 public:
  SocketAddr(std::string&& host, std::string&& port) {
    CheckLinuxError(getaddrinfo(host.c_str(), port.c_str(), nullptr, &addrinfo_));
    if (addrinfo_ == nullptr) {
      throw std::logic_error("getaddrinfo failed");
    }
  }
  SocketAddr(SocketAddr&& other) noexcept : addrinfo_(other.addrinfo_) { other.addrinfo_ = nullptr; }
  ~SocketAddr() { freeaddrinfo(addrinfo_); }
  void GetNextEntry() {
    if (!addrinfo_->ai_next) {
      throw std::logic_error("No more entries");
    }
    addrinfo_ = addrinfo_->ai_next;
  }
  int CreateSocket(const SocketType type = SocketType::TCP) const {
    int sockfd_;
    if (type == SocketType::UDP) {
      sockfd_ = CheckLinuxError(socket(addrinfo_->ai_family, SOCK_DGRAM|SOCK_NONBLOCK, 0));
    } else {
      sockfd_ = CheckLinuxError(socket(addrinfo_->ai_family, SOCK_STREAM|SOCK_NONBLOCK, 0));
    }
    int flag = 1;
    //默认低水位是1就是最好的
    // int flag1 = 2;
    setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
    // setsockopt(sockfd_, SOL_SOCKET, SO_RCVLOWAT, &flag1, sizeof(flag1));
    // setsockopt(sockfd_, SOL_SOCKET, SO_SNDLOWAT, &flag1, sizeof(flag1));
    CheckLinuxError(bind(sockfd_, addrinfo_->ai_addr, addrinfo_->ai_addrlen));
    if (type == SocketType::TCP) {
      CheckLinuxError(listen(sockfd_, SOMAXCONN));
    }
    return sockfd_;
  }

  addrinfo* addrinfo_;
};
}  // namespace lhy
#endif  // SOCKET_ADDR_H
