#include <sys/epoll.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

// 文件事件结构体
typedef struct aeFileEvent {
  int mask; /* 事件掩码，可以是 AE_READABLE（可读）、AE_WRITABLE（可写）或 AE_BARRIER（屏障）之一 */
  aeFileProc *rfileProc; // 读事件处理器
  aeFileProc *wfileProc; // 写事件处理器
  void *clientData;      // 客户端私有数据
} aeFileEvent;

/* 时间事件结构体 */
typedef struct aeTimeEvent {
  long long id; /* 时间事件标识符 */
  monotime when; // 事件触发时间
  aeTimeProc *timeProc; // 时间事件处理器
  aeEventFinalizerProc *finalizerProc; // 事件终结器处理器
  void *clientData; // 客户端私有数据
  struct aeTimeEvent *prev; // 前一个时间事件
  struct aeTimeEvent *next; // 后一个时间事件
  int refcount; /* 引用计数，用于防止在递归时间事件调用中释放定时器事件 */
} aeTimeEvent;

/* 已触发事件结构体 */
typedef struct aeFiredEvent {
  int fd;   // 文件描述符
  int mask; // 事件掩码
} aeFiredEvent;

typedef struct aeEventLoop {
  int maxfd;   // 当前注册的最高文件描述符
  int setsize; // 跟踪的文件描述符的最大数量
  long long timeEventNextId; // 下一个时间事件的ID
  int nevents;         // 已注册事件的数量
  aeFileEvent *events; /* 已注册的文件事件 */
  aeFiredEvent *fired; /* 已触发的文件事件 */
  aeTimeEvent *timeEventHead; // 时间事件链表头
  int stop; // 事件循环停止标志
  void *apidata; /* 用于存储特定于轮询API的数据 */
  aeBeforeSleepProc *beforesleep; // 进入休眠前的回调函数
  aeBeforeSleepProc *aftersleep;  // 退出休眠后的回调函数
  int flags; // 事件循环标志
  void *privdata[2]; // 私有数据
} aeEventLoop;

// epoll API状态结构体
typedef struct aeApiState {
  int epfd; // epoll 实例的文件描述符
  struct epoll_event *events; // epoll 事件数组
} aeApiState;

// 创建 epoll API 状态
static int aeApiCreate(aeEventLoop *eventLoop) {
  aeApiState *state = zmalloc(sizeof(aeApiState)); // 分配 aeApiState 结构体内存

  if (!state) return -1; // 如果分配失败，返回 -1
  // 分配 epoll 事件数组内存，大小为 eventLoop->setsize
  state->events = zmalloc(sizeof(struct epoll_event) * eventLoop->setsize);
  if (!state->events) {
    zfree(state); // 如果分配失败，释放 state 内存
    return -1;    // 返回 -1
  }
  // 创建 epoll 实例，1024 只是给内核的一个提示
  state->epfd = epoll_create(1024);
  if (state->epfd == -1) {
    zfree(state->events); // 如果创建失败，释放 events 内存
    zfree(state);         // 释放 state 内存
    return -1;            // 返回 -1
  }
  anetCloexec(state->epfd); // 设置 epoll 文件描述符为 CLOEXEC
  eventLoop->apidata = state; // 将 epoll 状态保存到事件循环中
  return 0;                   // 返回 0 表示成功
}

// 调整 epoll 事件数组的大小
static int aeApiResize(aeEventLoop *eventLoop, int setsize) {
  aeApiState *state = eventLoop->apidata; // 获取 epoll API 状态

  // 重新分配 epoll 事件数组内存
  state->events = zrealloc(state->events, sizeof(struct epoll_event)*setsize);
  return 0; // 返回 0 表示成功
}

// 释放 epoll API 状态
static void aeApiFree(aeEventLoop *eventLoop) {
    aeApiState *state = eventLoop->apidata; // 获取 epoll API 状态

    close(state->epfd); // 关闭 epoll 文件描述符
    zfree(state->events); // 释放 epoll 事件数组内存
    zfree(state);         // 释放 epoll API 状态内存
}
#define AE_OK 0
#define AE_ERR -1

#define AE_NONE 0     /* No events registered. */
#define AE_READABLE 1 /* Fire when descriptor is readable. */
#define AE_WRITABLE 2 /* Fire when descriptor is writable. */
#define AE_BARRIER                                \
  4 /* With WRITABLE, never fire the event if the \
READABLE event already fired in the same event    \
loop iteration. Useful when you want to persist   \
things to disk before sending replies, and want   \
to do that in a group fashion. */

#define AE_FILE_EVENTS (1 << 0)
#define AE_TIME_EVENTS (1 << 1)
#define AE_ALL_EVENTS (AE_FILE_EVENTS | AE_TIME_EVENTS)
#define AE_DONT_WAIT (1 << 2)
#define AE_CALL_BEFORE_SLEEP (1 << 3)
#define AE_CALL_AFTER_SLEEP (1 << 4)
// 添加或修改文件事件
static int aeApiAddEvent(aeEventLoop *eventLoop, int fd, int mask) {
  aeApiState *state = eventLoop->apidata;  // 获取 epoll API 状态
  struct epoll_event ee = {0};             /* 避免 valgrind 警告，初始化为0 */
  /* 如果文件描述符已经注册过事件，则执行 MOD（修改）操作。
   * 否则执行 ADD（添加）操作。 */
  int op = eventLoop->events[fd].mask == AE_NONE ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;

  ee.events = 0;
  mask |= eventLoop->events[fd].mask;                        /* 合并旧事件 */
  if (mask & AE_READABLE) ee.events |= EPOLLIN;              // 如果是可读事件，设置 EPOLLIN
  if (mask & AE_WRITABLE) ee.events |= EPOLLOUT;             // 如果是可写事件，设置 EPOLLOUT
  ee.data.fd = fd;                                           // 设置事件关联的文件描述符
  if (epoll_ctl(state->epfd, op, fd, &ee) == -1) return -1;  // 调用 epoll_ctl 添加或修改事件
  return 0;                                                  // 返回 0 表示成功
}

// 删除文件事件
static void aeApiDelEvent(aeEventLoop *eventLoop, int fd, int delmask) {
    aeApiState *state = eventLoop->apidata; // 获取 epoll API 状态
    struct epoll_event ee = {0}; /* 避免 valgrind 警告，初始化为0 */
    // 计算删除 delmask 后剩余的事件掩码
    int mask = eventLoop->events[fd].mask & (~delmask);

    ee.events = 0;
    if (mask & AE_READABLE) ee.events |= EPOLLIN; // 如果剩余可读事件，设置 EPOLLIN
    if (mask & AE_WRITABLE) ee.events |= EPOLLOUT; // 如果剩余可写事件，设置 EPOLLOUT
    ee.data.fd = fd; // 设置事件关联的文件描述符
    if (mask != AE_NONE) {
        // 如果还有其他事件，则修改事件
        epoll_ctl(state->epfd,EPOLL_CTL_MOD,fd,&ee);
    } else {
        /* 注意：内核 < 2.6.9 版本即使是 EPOLL_CTL_DEL 操作也需要非空的事件指针。 */
        // 如果没有其他事件，则删除事件
        epoll_ctl(state->epfd,EPOLL_CTL_DEL,fd,&ee);
    }
}

// epoll 事件轮询
static int aeApiPoll(aeEventLoop *eventLoop, struct timeval *tvp) {
    aeApiState *state = eventLoop->apidata; // 获取 epoll API 状态
    int retval, numevents = 0;

    // 调用 epoll_wait 等待事件
    retval = epoll_wait(state->epfd,state->events,eventLoop->setsize,
            tvp ? (tvp->tv_sec*1000 + (tvp->tv_usec + 999)/1000) : -1);
    if (retval > 0) {
        int j;

        numevents = retval; // 获取触发的事件数量
        for (j = 0; j < numevents; j++) {
            int mask = 0;
            struct epoll_event *e = state->events+j; // 获取当前触发的 epoll 事件

            if (e->events & EPOLLIN) mask |= AE_READABLE; // 如果是 EPOLLIN，设置为可读
            if (e->events & EPOLLOUT) mask |= AE_WRITABLE; // 如果是 EPOLLOUT，设置为可写
            if (e->events & EPOLLERR) mask |= AE_WRITABLE|AE_READABLE; // 如果是错误，设置为可读可写
            if (e->events & EPOLLHUP) mask |= AE_WRITABLE|AE_READABLE; // 如果是挂起，设置为可读可写
            eventLoop->fired[j].fd = e->data.fd; // 记录触发事件的文件描述符
            eventLoop->fired[j].mask = mask; // 记录触发事件的掩码
        }
    } else if (retval == -1 && errno != EINTR) {
        // 如果 epoll_wait 返回错误且不是被中断，则触发 panic
        panic("aeApiPoll: epoll_wait, %s", strerror(errno));
    }

    return numevents; // 返回触发的事件数量
}

// 获取 epoll API 名称
static char *aeApiName(void) {
    return "epoll"; // 返回 "epoll"
}