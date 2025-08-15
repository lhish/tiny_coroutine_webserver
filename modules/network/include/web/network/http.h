//
// Created by lhy on 24-9-26.
//

#ifndef HTTP_H
#define HTTP_H
#include <functional>
#include <map>
#include <sstream>
#include <string>

#include "help_func.h"
namespace lhy {

class HtmlSolver_ {
 public:
  std::map<std::string, std::string> parseQuery(const std::string& query) {
    std::map<std::string, std::string> params;
    std::istringstream stream(query);
    std::string keyValue;

    while (std::getline(stream, keyValue, '&')) {
      auto pos = keyValue.find('=');
      if (pos != std::string::npos) {
        std::string key = urlDecode(keyValue.substr(0, pos));
        std::string value = urlDecode(keyValue.substr(pos + 1));
        params[key] = value;
      }
    }
    return params;
  }
  std::map<std::string, std::string> parseMultipart(const std::string& body) {
    std::map<std::string, std::string> formData;
    std::string delimiter = "Content-Disposition: form-data; name=\"";
    std::size_t start = 0;

    while ((start = body.find(delimiter, start)) != std::string::npos) {
      start += delimiter.length();
      std::size_t nameEnd = body.find("\"", start);
      if (nameEnd == std::string::npos) break;

      std::string name = body.substr(start, nameEnd - start);
      start = nameEnd + 5;  // Skip past the closing quote and CRLF

      std::size_t contentEnd = body.find("\r\n", start);
      if (contentEnd == std::string::npos) break;

      std::string content = body.substr(start, contentEnd - start);
      formData[urlDecode(name)] = urlDecode(content);

      start = contentEnd + 2;  // Move past the CRLF to the next part
    }

    return formData;
  }

  std::string operator()(const std::string& req) {
    std::string method;
    if (req.starts_with("GET ")) {
      method = "GET";
    } else if (req.starts_with("POST ")) {
      method = "POST";
    } else {
      return "Unsupported request method.";
    }

    std::string path;
    std::map<std::string, std::string> params;

    if (method == "GET") {
      path = req.substr(req.find("GET /") + 5, req.find(" HTTP/1.1") - 5);
      auto queryStart = path.find('?');
      if (queryStart != std::string::npos) {
        params = parseQuery(path.substr(queryStart + 1));  // Extract query parameters
        path = path.substr(0, queryStart);                 // Update path
      }
    } else if (method == "POST") {
      path = req.substr(req.find("POST /") + 6, req.find(" HTTP/1.1") - 6);
      auto body_pos = req.find("\r\n\r\n");
      if (body_pos != std::string::npos) {
        std::string contentType = req.substr(req.find("Content-Type: ") + 14);

        if (contentType.starts_with("multipart/form-data")) {
          // Handle multipart form data
          std::string boundary = contentType.substr(contentType.find("boundary=") + 9);
          std::string body = req.substr(body_pos + 4);  // Skip headers
          auto multipartParams = parseMultipart(body);
          params.insert(multipartParams.begin(), multipartParams.end());  // Combine with existing params
        } else {
          // Handle regular form data
          std::string body = req.substr(body_pos + 4);  // Skip headers
          params = parseQuery(body);
        }
      }
    }
    path = '/' + path;
    // Continue processing based on the path and parameters
    if (router.contains(path)) {
      return router[path](std::move(params));  // Pass the entire params map to the handler
    }

    return ReadStaticFile(base_path + path);  // Fallback to static HTML
  }

  std::function<std::string(std::map<std::string, std::string>&&)>& GetRouter(const std::string& path) {
    return router[path];
  }
  std::map<std::string, std::function<std::string(std::map<std::string, std::string>&&)>> router;
  std::string base_path;
};
HtmlSolver_& GetHtmlSolver() {
  static HtmlSolver_ solver;
  return solver;
}
std::string HtmlSolver(const std::string& req) { return GetHtmlSolver()(req); }

class Router_ {
 public:
  std::function<std::string(std::map<std::string, std::string>&&)>& operator()(const std::string& path) {
    return GetHtmlSolver().GetRouter(path);
  }
};
std::function<std::string(std::map<std::string, std::string>&&)>& Router(const std::string& path) {
  static Router_ router;
  return router(path);
}
}  // namespace lhy
#endif  // HTTP_H
