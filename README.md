# Tiny_Coroutine_WebServer

## 项目概述

该项目是一个基于C++20/23协程和Linux epoll事件驱动模型构建的高性能Web服务器。它集成了MySQL数据库功能，能够处理HTTP请求（GET/POST），解析查询参数和multipart表单数据，并支持自定义路由。本项目旨在提供一个高效、可扩展的后端解决方案，适用于需要处理高并发网络请求的Web应用。

## 核心功能

*   **高性能网络I/O**: 利用Linux epoll机制实现高效的事件循环，以非阻塞方式处理大量的并发网络连接，确保服务器在高负载下的响应能力。
*   **异步编程**: 采用C++20/23协程（Coroutines）实现异步操作，显著提升服务器的并发处理能力和响应速度，避免传统线程模型的开销。
*   **数据库交互**: 集成MySQL Connector/C++库，支持与MySQL数据库进行高效的数据存储和检索。这使得服务器能够轻松实现用户认证、数据持久化等功能。
*   **HTTP服务**: 提供全面的HTTP请求解析（支持GET/POST请求）、灵活的路由机制和静态文件服务。能够轻松构建动态Web应用和RESTful API。
*   **Web前端支持**: 包含基础的HTML、CSS和JavaScript前端页面，可作为示例或快速启动项目，实现用户交互界面。

## 技术栈

*   **核心语言**: C++ (C++20/23)
*   **构建系统**: CMake
*   **网络I/O**: Linux系统编程 (epoll)
*   **数据库**: MySQL Connector/C++
*   **Web协议**: HTTP
*   **前端技术**: HTML, CSS, JavaScript

## 项目结构

```
.
├── CMakeLists.txt
├── main.cpp
├── README.md
├── modules/
│   ├── coroutine/       # 协程模块
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   └── src/
│   ├── databases/       # 数据库模块 (MySQL Connector/C++)
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   └── src/
│   └── network/         # 网络I/O模块 (epoll, HTTP)
│       ├── CMakeLists.txt
│       ├── include/
│       └── src/
├── http/                # 静态Web文件 (HTML, CSS, JS)
│   ├── index.html
│   ├── login.html
│   ├── register.html
├── test/                # 单元测试和示例
└── database_config_example.json # 数据库配置示例
```

## 配置

**配置数据库**:
    复制 `database_config_example.json` 为 `database_config.json` 并根据您的MySQL数据库配置进行修改。
## 许可证

本项目采用 [LICENSE](LICENSE) 文件中定义的许可证。
