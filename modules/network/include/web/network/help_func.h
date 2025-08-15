//
// Created by lhy on 24-9-26.
//

#ifndef HELP_FUNC_H
#define HELP_FUNC_H
#include <cstring>
#include <fstream>
#include <string>

#include "web/coroutine/debug.h"

namespace lhy {
inline char hexCharToDec(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;  // Invalid input
}

// URL解码函数
inline std::string urlDecode(const std::string& url) {
  std::ostringstream result;
  for (size_t i = 0; i < url.length(); ++i) {
    char ch = url[i];
    if (ch == '%') {
      if (i + 2 < url.length()) {
        // 读取接下来的两个字符
        char hex1 = url[i + 1];
        char hex2 = url[i + 2];

        // 将这两个字符转换为对应的数字并还原成字符
        char decodedChar = hexCharToDec(hex1) * 16 + hexCharToDec(hex2);
        result << decodedChar;

        // 跳过处理过的两个字符
        i += 2;
      }
    } else if (ch == '+') {
      // '+' 在 URL 编码中表示空格
      result << ' ';
    } else {
      // 普通字符直接添加
      result << ch;
    }
  }
  return result.str();
}
inline int CheckLinuxError(int err) {
  if (err == -1) {
    debug(), "error:", strerror(errno);
  }
  return err;
}
inline std::string ReadStaticFile(const std::string& path) {
  static std::unordered_map<std::string , std::string> cache;
  if (cache.contains(path)) {
    return cache[path];
  }
  /// 读文件
  std::ifstream t(path);
  std::string content{std::istreambuf_iterator<char>(t), std::istreambuf_iterator<char>()};
  cache[path] = content;
  return content;
}
}
#endif //HELP_FUNC_H
