//
// Created by lhy on 25-7-29.
//

#ifndef CO_H
#define CO_H

#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <coroutine>
#include <exception>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>

#include "debug.h"
template <typename T>
concept Awaiter = requires(T t) {
  { t.await_ready() } -> std::convertible_to<bool>;
  { t.await_suspend(std::coroutine_handle{}) };
  { t.await_resume() };
};
template <typename T>
concept CoAwaiter = requires(T t) {
  { t.operator co_await() } -> Awaiter;
};
template <typename T>
concept Awaitable = CoAwaiter<T> || Awaiter<T>;
template <Awaitable T>
class AwaitableTypeHelper {};
template <Awaiter T>
class AwaitableTypeHelper<T> {
 public:
  using Type = decltype(std::declval<T>().await_resume());
};
template <CoAwaiter T>
class AwaitableTypeHelper<T> {
 public:
  using Type = decltype(std::declval<T>().operator co_await().await_resume());
};
template <Awaitable T>
using AwaitableType = typename AwaitableTypeHelper<T>::Type;
class LoopBase {
 public:
  virtual ~LoopBase() = default;
  virtual void Execute(const std::function<void()>& handle,
                       std::chrono::high_resolution_clock::time_point time_point) = 0;
  void Execute(const std::function<void()>& handle) { Execute(handle, std::chrono::high_resolution_clock::now()); }
  virtual void Execute(const std::function<void()>& func, int fd, EPOLL_EVENTS events, std::string& buf, size_t size) {}
};

// 当使用loop的时候协程就已经不是为了方便或者之类的，其实就类似于线程了
class NormalLoop : public LoopBase {
 public:
  NormalLoop() {
    event_ = std::make_unique<epoll_event[]>(max_event_);
    // 创建 epoll 实例，1024 只是给内核的一个提示
    epfd_ = epoll_create(max_event_);
    if (epfd_ == -1) {
      throw std::runtime_error("epoll_create failed");
    }
    // anetCloexec(state->epfd); // 设置 epoll 文件描述符为 CLOEXEC
    std::jthread loop_thread(&NormalLoop::RunAll, this);
    loop_thread.detach();
  }
  void Execute(const std::function<void()>& handle,
               std::chrono::high_resolution_clock::time_point time_point) override {
    mutex_.lock();
    time_queue_.emplace(time_point, handle);
    mutex_.unlock();
  }
  [[noreturn]] void RunAll() {
    using namespace std::chrono_literals;
    while (true) {
      bool count = false;
      mutex_.lock();
      while (!time_queue_.empty()) {
        count = true;
        auto [time_point, handle] = time_queue_.top();
        mutex_.unlock();
        if (time_point > std::chrono::high_resolution_clock::now()) {
          std::this_thread::sleep_for(1ns);
          break;
        }
        mutex_.lock();
        time_queue_.pop();
        mutex_.unlock();
        if (!handle) {
          throw std::runtime_error("time_queue_ error");
        }
        handle();
        mutex_.lock();
      }
      mutex_.unlock();
      const int retval = epoll_wait(epfd_, event_.get(), max_event_, 0);
      for (int j = 0; j < retval; j++) {
        count = true;
        auto& [events, data] = event_[j];
        int fd = data.fd;
        auto& [func, size, event, str] = size_map.at(fd);
        char buf[1024];
        if (size == -1) {
          func();
          Delete(data.fd);
        } else if (events == EPOLLIN) {
          int n;
          while (true) {
            n = recv(data.fd, &buf, 1024, MSG_DONTWAIT);
            if (n < 0) {
              if (errno == EWOULDBLOCK) {
                break;
              }
            } else if (n == 0) {
              //其实也可以选择里面把它关了？
              break;
            }
            str += std::string(buf, n);
          }
          if (str.length() >= size || n == -1) {
            func();
            Delete(data.fd);
          }
        } else {
          while (!str.empty()) {
            const int n = send(data.fd, str.c_str(), str.length(), MSG_DONTWAIT);
            if (n < 0) {
              if (errno == EWOULDBLOCK||errno==EAGAIN) {
                break;
              }
              //需要把errno返回去才行，read也是，有的错误不能在这里处理
              debug(),errno;
            } else if (n == 0) {
              break;
            }
            str = str.substr(n);
          }
          if (str.empty()) {
            func();
            Delete(data.fd);
          }
        }
      }
      if (!count) {
        std::this_thread::sleep_for(1ns);
      }
    }
  }
  void Add(const int fd, const EPOLL_EVENTS events) const {
    epoll_event ee{};
    ee.events = 0;
    ee.events |= events;
    ee.events |= EPOLLET;
    ee.data.fd = fd;
    while (epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ee) == -1) {
    }
  }
  void Delete(const int fd) {
    epoll_event ee{};
    ee.events = 0;
    ee.events |= std::get<2>(size_map.at(fd));
    ee.events |= EPOLLET;
    ee.data.fd = fd;
    if (epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, &ee) == -1) {
      throw std::runtime_error("epoll_ctl del fail");
    }
    size_map.erase(fd);
  }
  void Execute(const std::function<void()>& func, const int fd, const EPOLL_EVENTS events, std::string& str,
               const size_t size) override {
    if (!func) {
      throw std::runtime_error("func is null");
    }
    size_map.emplace(fd,
                     std::tuple<std::function<void()>, size_t, EPOLL_EVENTS, std::string&>{func, size, events, str});
    Add(fd, events);
  }
  ~NormalLoop() override { close(epfd_); }

 private:
  int epfd_;
  inline static int max_event_{40096};
  std::unique_ptr<epoll_event[]> event_;
  std::unordered_map<int, std::tuple<std::function<void()>, size_t, EPOLL_EVENTS, std::string&>> size_map;
  struct time_queue_object {
    std::chrono::high_resolution_clock::time_point first;
    std::function<void()> second;
    friend bool operator<(const time_queue_object& lhs, const time_queue_object& rhs) { return lhs.first < rhs.first; }
    friend bool operator<=(const time_queue_object& lhs, const time_queue_object& rhs) { return !(rhs < lhs); }
    friend bool operator>(const time_queue_object& lhs, const time_queue_object& rhs) { return rhs < lhs; }
    friend bool operator>=(const time_queue_object& lhs, const time_queue_object& rhs) { return !(lhs < rhs); }
  };
  std::mutex mutex_;
  std::priority_queue<time_queue_object, std::vector<time_queue_object>, std::greater<>> time_queue_;
};
class SharedNormalLoop : public LoopBase {
 public:
  void Execute(const std::function<void()>& handle,
               const std::chrono::high_resolution_clock::time_point time_point) override {
    loop.Execute(handle, time_point);
  }
  void Execute(const std::function<void()>& func, const int fd, EPOLL_EVENTS events, std::string& buf,
               size_t size) override {
    loop.Execute(func, fd, events, buf, size);
  }
  inline static NormalLoop loop{};
};
class NoopLoop : public LoopBase {
 public:
  void Execute(const std::function<void()>& handle,
               const std::chrono::high_resolution_clock::time_point time_point) override {
    handle();
  }
};
template <typename T>
concept Looper = std::is_base_of_v<LoopBase, T>;
template <typename T, Looper Loop = SharedNormalLoop>
class Task;
template <typename T, Looper Loop>
class TaskAwaiter {
 public:
  using RetType = T;
  bool await_ready() const noexcept { return false; }
  // 当返回void的时候返回other的调用者处
  // 当返回bool的时候true返回other的调用者处,false返回other调用本awaiter处
  // 当返回一个协程句柄han时，即调用han.resume()//如果是std::noop_coroutine，那么效果等价于void和true
  // 当有异常时，直接抛出，在other本awaiter处抛出
  void await_suspend(std::coroutine_handle<> other) noexcept {
    task_();
    task_.Finally(executor_, [other] {
      // debug(),typeid(T{}).name();
      other();
    });
  }
  T await_resume() noexcept { return task_.get_or_throw(); }
  TaskAwaiter(Task<T, Loop>& task, LoopBase* executor) : task_(task, false), executor_(executor) {}

 private:
  Task<T, Loop> task_;
  LoopBase* executor_;  // other的executor
};

class EpollAwaiter {
 public:
  friend class EpollAwaiterImpl;
  EpollAwaiter(const int fd, const EPOLL_EVENTS events, std::string str, const size_t size = -1)
      : str_(std::move(str)), fd_(fd), events_(events), size_(size) {}

 private:
  std::string str_;
  int fd_;
  EPOLL_EVENTS events_;
  size_t size_;
};
class EpollAwaiterImpl {
 public:
  bool await_ready() const noexcept { return false; }
  // 当返回void的时候返回other的调用者处
  // 当返回bool的时候true返回other的调用者处,false返回other调用本awaiter处
  // 当返回一个协程句柄han时，即调用han.resume()//如果是std::noop_coroutine，那么效果等价于void和true
  // 当有异常时，直接抛出，在other本awaiter处抛出
  void await_suspend(std::coroutine_handle<> other) noexcept {
    own_executor_.Execute([other, this] { executor_->Execute([other] { other(); }); }, task_.fd_, task_.events_,
                          task_.str_, task_.size_);
  }
  std::string await_resume() noexcept { return task_.str_; }
  EpollAwaiterImpl(EpollAwaiter&& task, LoopBase* executor) : task_(std::move(task)), executor_(executor) {}

 private:
  SharedNormalLoop own_executor_;
  EpollAwaiter task_;
  LoopBase* executor_;  // other的executor
};
template <Looper Loop>
class SleepAwaiter {
 public:
  using RetType = void;
  bool await_ready() const noexcept { return false; }
  // 当返回void的时候返回other的调用者处
  // 当返回bool的时候true返回other的调用者处,false返回other调用本awaiter处
  // 当返回一个协程句柄han时，即调用han.resume()//如果是std::noop_coroutine，那么效果等价于void和true
  // 当有异常时，直接抛出，在other本awaiter处抛出
  void await_suspend(std::coroutine_handle<> other) noexcept {
    executor_->Execute([other] { other(); }, std::chrono::high_resolution_clock::now() + duration_);
  }
  void await_resume() noexcept {}
  SleepAwaiter(const std::chrono::high_resolution_clock::duration duration, LoopBase* executor)
      : duration_(duration), executor_(executor) {}

 private:
  std::chrono::high_resolution_clock::duration duration_;
  LoopBase* executor_;
};

// template <typename ... T>
// class WhenAllAwaiterImpl;
// template <typename ... T>
// class WhenAllAwaiter;
template <typename T, Looper Loop = SharedNormalLoop>
struct TaskPromise {
  friend class Task<T, Loop>;
  using handle_type = std::coroutine_handle<TaskPromise>;
  Task<T, Loop> get_return_object() { return {&executor_, handle_type::from_promise(*this), true}; }
  // co_await awaiter:
  // if (!awaiter.ready()){
  //     awaiter.await_suspend(handle_type::current());
  // }
  // 当下一次进入协程的时候或者没有suspend继续执行
  // auto ret=awaiter.await_resume();
  std::suspend_always
  initial_suspend() noexcept {  // always_suspend的await_suspend的返回时void，也就是回到等待suspend_always的协程的调用者那里也就是挂起本协程
    return {};
  }  // 用来获取初等待体，在协程开始时会被调用并对于这个等待体co await
  std::suspend_always final_suspend() noexcept {
    return {};
  }  // 末等待体，在函数return后立即被调用，用来获取末等待体，并co await，最后会自动destroy整个协程句柄和相关frame
  // 不管返回什么，final_suspend都应该挂起！！！然后好像也是在这个时候done为true了，但是如果不挂起一切基于handle的判断都会烂掉
  //  void return_void() {}//与return_value二选一，在协程函数return时执行
  void return_value(T ret) {
    ret_ = ret;
    for (auto& [executor, each_final] : final_) {
      executor->Execute(each_final);
    }
    final_.clear();
  }
  void unhandled_exception() {
    except_ = std::current_exception();
  }  // 在抛出异常的时候会被调用，接着便进入final_suspend
  std::suspend_always yield_value(
      T ret) {  // 在co_yield expr时会执行这个函数yield_value(expr)并获取对应的等待体并co_await
    ret_.emplace(ret);
    for (auto& [executor, each_final] : final_) {
      executor->Execute(each_final);
    }
    final_.clear();
    return {};
  }

  template <typename U, Looper LoopU>
  TaskAwaiter<U, LoopU> await_transform(Task<U, LoopU>& task) {
    return TaskAwaiter<U, LoopU>(task, &executor_);
  }
  template <typename U, Looper LoopU>
  TaskAwaiter<U, LoopU> await_transform(Task<U, LoopU>&& task) {
    return TaskAwaiter<U, LoopU>(task, &executor_);
  }
  SleepAwaiter<Loop> await_transform(std::chrono::high_resolution_clock::duration&& duration) {
    return SleepAwaiter<Loop>(duration, &executor_);
  }
  EpollAwaiterImpl await_transform(EpollAwaiter&& awaiter) { return EpollAwaiterImpl(std::move(awaiter), &executor_); }
  // template <typename ...U>
  // WhenAllAwaiter<U...> await_transform(WhenAllAwaiterImpl<U...>&& task) {
  //   return WhenAllAwaiter<U...>(task, &executor_);
  // }
  void Finally(LoopBase* executor, const std::function<void()>& func) { final_.emplace_back(executor, func); }

 private:
  std::vector<std::pair<LoopBase*, std::function<void()>>> final_;
  std::optional<T> ret_;
  std::exception_ptr except_;
  Loop executor_;
};
template <Looper Loop>
struct TaskPromise<void, Loop> {
  friend class Task<void, Loop>;
  using handle_type = std::coroutine_handle<TaskPromise>;
  Task<void, Loop> get_return_object() { return {&executor_, handle_type::from_promise(*this), true}; }
  // co_await awaiter:
  // if (!awaiter.ready()){
  //     awaiter.await_suspend(handle_type::current());
  // }
  // 当下一次进入协程的时候或者没有suspend继续执行
  // auto ret=awaiter.await_resume();
  std::suspend_always
  initial_suspend() noexcept {  // always_suspend的await_suspend的返回时void，也就是回到等待suspend_always的协程的调用者那里也就是挂起本协程
    return {};
  }  // 用来获取初等待体，在协程开始时会被调用并对于这个等待体co await
  std::suspend_always final_suspend() noexcept {
    return {};
  }  // 末等待体，在函数return后立即被调用，用来获取末等待体，并co await，最后会自动destroy整个协程句柄和相关frame
  // 不管返回什么，final_suspend都应该挂起！！！然后好像也是在这个时候done为true了，但是如果不挂起一切基于handle的判断都会烂掉
  //  void return_void() {}//与return_value二选一，在协程函数return时执行
  void return_void() {
    for (auto& [executor, each_final] : final_) {
      executor->Execute(each_final);
    }
    final_.clear();
  }
  void unhandled_exception() {
    except_ = std::current_exception();
  }  // 在抛出异常的时候会被调用，接着便进入final_suspend
  template <typename U, Looper LoopU>
  TaskAwaiter<U, LoopU> await_transform(Task<U, LoopU>& task) {
    return TaskAwaiter<U, LoopU>(task, &executor_);
  }
  template <typename U, Looper LoopU>
  TaskAwaiter<U, LoopU> await_transform(Task<U, LoopU>&& task) {
    return TaskAwaiter<U, LoopU>(task, &executor_);
  }
  SleepAwaiter<Loop> await_transform(std::chrono::high_resolution_clock::duration&& duration) {
    return SleepAwaiter<Loop>(duration, &executor_);
  }
  EpollAwaiterImpl await_transform(EpollAwaiter&& awaiter) { return EpollAwaiterImpl(std::move(awaiter), &executor_); }
  // template <typename U, Looper LoopU>
  // WhenAllAwaiter<LoopU> await_transform(WhenAllAwaiterImpl<U, LoopU>&& task) {
  //   return WhenAllAwaiter<U, LoopU>(task, &executor_);
  // }
  void Finally(LoopBase* executor, const std::function<void()>& func) { final_.emplace_back(executor, func); }

 private:
  std::vector<std::pair<LoopBase*, std::function<void()>>> final_;
  std::exception_ptr except_;
  Loop executor_;
};
template <typename T, Looper Loop>
class Task {
 public:
  using promise_type = TaskPromise<T, Loop>;
  Task(const Task& other) = delete;
  Task(Task&& other) noexcept
      : handle_(std::move(other.handle_)), executor_(other.executor_), owner_(std::exchange(other.owner_, {})) {}
  Task& operator=(const Task& other) = delete;
  Task& operator=(Task&& other) noexcept {
    if (this == &other) return *this;
    handle_ = std::move(other.handle_);
    executor_ = other.executor_;
    owner_ = std::exchange(other.owner_, {});
    return *this;
  }
  Task(Task& other, const bool owner = false) {
    executor_ = other.executor_;
    handle_ = other.handle_;
    if (owner) {
      if (other.owner_) {
        other.owner_ = false;
      }
    }
    owner_ = owner;
  }
  // 一种可能是在协程函数return时，会自动调用，用于销毁协程句柄和frame，但实际上只有当final_suspend为never的时候才会触发，而一旦触发再次destroy就会导致双重释放
  // 因此要么never+不析构并且不超过协程的可重入次数，要么always+析构+suspend的时候handle.done()
  ~Task() {
    if (owner_) {
      handle_.destroy();
    }
  }
  using handle_type = typename promise_type::handle_type;

  operator bool() const { return !handle_.done(); }  // 判断协程是否结束
  // 有两种获取返回值的方式，一种是到promise中去获取，另一种是在co_await的时候会调用awaiter的await_resume去获取这个等待体的返回值
  void operator()() {
    if (!handle_.done()) {
      if constexpr (!std::is_void_v<T>) {
        handle_.promise().ret_ = {};
      }
      executor_->Execute(handle_);
    } else {
      throw std::runtime_error("task is done");
    }
  }  // 运行协程，实际上根本运行协程的是handle()
  void Finally(LoopBase* executor, const std::function<void()>& func) { handle_.promise().Finally(executor, func); }

  Task(LoopBase* loop, const handle_type& handle, const bool owner) : handle_(handle), executor_(loop), owner_(owner) {}

 private:
  handle_type handle_;
  LoopBase* executor_{};  // 自己的executor
  bool owner_;

 public:
  T get_or_throw() {
    using namespace std::chrono_literals;
    if (handle_.promise().except_) {
      std::rethrow_exception(handle_.promise().except_);
    }
    if constexpr (std::is_void_v<T>) {
      while (!handle_.done()) {
        std::this_thread::sleep_for(1ns);
      }
      return;
    } else {
      auto& ret = handle_.promise().ret_;
      while (!ret) {
        std::this_thread::sleep_for(1ns);
      }
      return *ret;
    }
  }
};
// template <typename ... T>
// class WhenAllAwaiterImpl {
// public:
//   explicit WhenAllAwaiterImpl(T&&... task) : task_(std::forward<T>(task)...) {}
//   std::tuple<T...> task_;
// };
// template <typename... T>
// WhenAllAwaiterImpl<T...> WhenAll(T&&... tasks) {
//   return {std::forward<T>(tasks)...};
// }
// template <typename ... T>
// class WhenAllAwaiter {
// public:
//   using RetType= std::tuple<typename T::RetType...>;
//   bool await_ready() const noexcept { return false; }
//   // 当返回void的时候返回other的调用者处
//   // 当返回bool的时候true返回other的调用者处,false返回other调用本awaiter处
//   // 当返回一个协程句柄han时，即调用han.resume()//如果是std::noop_coroutine，那么效果等价于void和true
//   // 当有异常时，直接抛出，在other本awaiter处抛出
//
//   template <size_t Is>
//   Task<void,NoopLoop> await_suspend_impl_impl_task(std::coroutine_handle<> other) noexcept {
//     co_await std::get<Is>(impl_.task_);
//   }
//   template <size_t Is>
//   void await_suspend_impl_impl(std::coroutine_handle<> other) noexcept {
//     auto task=await_suspend_impl_impl_task<Is>(other);
//     task.Finally(executor_, [other, this] {
//       count_--;
//       if (count_ == 0) {
//         other();
//       }
//     });
//     task();
//   }
//   template <size_t... Is>
//   void await_suspend_impl(const std::index_sequence<Is...>& seq, std::coroutine_handle<> other) noexcept {
//     (await_suspend_impl_impl<Is>(other), ...);
//   }
//   void await_suspend(std::coroutine_handle<> other) noexcept {
//     await_suspend_impl(std::make_index_sequence<sizeof...(T)>{}, other);
//   }
//   template <size_t... Is>
//   void await_resume_impl(const std::index_sequence<Is...>& seq) noexcept {
//     (std::get<Is>(ret_) .operator=( std::get<Is>(impl_.task_).await_resume()),...);
//   }
//   std::tuple<typename T::RetType...> await_resume() noexcept {
//     await_resume_impl(std::make_index_sequence<sizeof...(T)>{});
//     return ret_;
//   }
//   WhenAllAwaiter(WhenAllAwaiterImpl<T...>&& impl, LoopBase* executor) : impl_(impl), executor_(executor) {}
//
// private:
//   int count_{sizeof...(T)};
//   WhenAllAwaiterImpl<T...> impl_;
//   std::tuple<typename T::RetType...> ret_;
//   LoopBase* executor_;  // other的executor
// };
#endif  // CO_H
