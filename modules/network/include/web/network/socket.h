//
// Created by lhy on 24-9-26.
//

#ifndef SOCKET_H
#define SOCKET_H
#include <netdb.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <list>
#include <string>
#include <thread>

#include "help_func.h"
#include "http.h"
#include "socket_addr.h"
#include "web/coroutine/co.h"
namespace lhy {

class Socket {
 public:
  Socket(std::string&& host, std::string&& port, std::string&& base_path, SocketType type = SocketType::TCP)
      : socket_addr_(std::move(host), std::move(port)) {
    GetHtmlSolver().base_path = base_path;
    sockfd_ = socket_addr_.CreateSocket(type);
    while (true) {
      if (type == SocketType::UDP) {
        char buf[1024];
        sockaddr_storage client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        std::string req;
        auto n = CheckLinuxError(recvfrom(sockfd_, buf, sizeof(buf), 0, (sockaddr*)&client_addr, &addr_len));
        req = std::string(buf, n);
        /// 解析一下html
        auto response = HtmlSolver(req);
        if (!response.starts_with("HTTP/1.1")) {
          response =
              "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " + std::to_string(response.size()) +
              "\r\n\r\n" + response;
        }
        CheckLinuxError(sendto(sockfd_, response.c_str(), response.size(), 0, (sockaddr*)&client_addr, addr_len));
      } else {
        auto f = accept_one();
        std::pmr::list<Task<void>> tasks;
        while (true) {
          f();
          for (auto fd = f.get_or_throw(); const auto& each_fd : fd) {
            tasks.emplace_back(solve_one(each_fd))();
          }
        }
      }
    }
  }
  Task<std::vector<int>> wait_accept() const {
    co_await EpollAwaiter(sockfd_, EPOLLIN, {});
    std::vector<int> fds;
    while (true) {
      //全错，这里后面两个是远程的连接
      if (auto ret = accept(sockfd_, socket_addr_.addrinfo_->ai_addr, &socket_addr_.addrinfo_->ai_addrlen); ret >= 0) {
        fds.emplace_back(ret);
      } else {
        break;
      }
    }
    co_return fds;
  }
  Task<std::string> wait_read(const int sockfd, const size_t size) const {
    auto ret = co_await EpollAwaiter(sockfd, EPOLLIN, {}, size);
    co_return ret;
  }
  //?
  Task<void> wait_write(const int sockfd, const std::string& str) {
    co_await EpollAwaiter(sockfd, EPOLLOUT, str, str.size());
    co_return;
  }
  Task<std::vector<int>> accept_one() const {
    while (true) {
      auto ret = co_await wait_accept();
      co_yield ret;
    }
  }
  Task<void> solve_one(const int client_fd) {
    while (true) {
      std::string req;
      std::optional<int> length;
      while (true) {
        auto buf = co_await wait_read(client_fd, 1024);
        req += buf;
        if (buf.empty()) {
          break;
        }
        if (!length) {
          if (const auto pos = req.find("\r\n\r\n"); pos != std::string::npos) {
            if (const auto content_length = req.find("Content-Length: "); content_length != std::string::npos) {
              length = std::stoi(req.substr(content_length + 16, req.find('\n', content_length + 16))) + pos + 4;
              if (req.size() >= *length) {
                break;
              }
            } else {
              break;
            }
          }
        } else {
          if (req.size() >= *length) {
            break;
          }
        }
      }
      ///需要再次裁切一手，将不是这一次req的部分放到下一次req的头上
      /// 解析一下html
      auto response = HtmlSolver(req);
      if (!response.starts_with("HTTP/1.1")) {
        response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " + std::to_string(response.size()) +
                   "\r\n\r\n" + response;
      }
      co_await wait_write(client_fd, response);
    }
  }

 private:
  int sockfd_;
  SocketAddr socket_addr_;
};
}  // namespace lhy
#endif  // SOCKET_H
