//
// Created by lhy on 24-9-21.
//
#include "web/network/http.h"

#include <filesystem>
#include <map>

#include "web/databases/database.h"
#include "web/network/help_func.h"
#include "web/network/socket.h"
using namespace lhy;
std::string base_path = "../http";
int main() {
  system("service mysql start");
  system("ifconfig");
  Router("/send_message") = [](std::map<std::string, std::string>&& query) {
    if (query.contains("message") && query.contains("username")) {
      DataBase db{};
      db.Open("chat", "message");
      db.InsertRow({"message"}, query["username"] + ": " + query["message"]);
    }
    return std::string();
  };

  Router("/messages") = [](std::map<std::string, std::string>&& query) {
    std::string chat;
    DataBase db{};
    db.Open("chat", "message");
    for (const auto& msg : db.SelectColumns(std::tuple{"message"}, {})) {
      chat += "<div class='message'>" + msg["message"].get<std::string>() + "</div>";
    }
    return chat;
  };

  Router("/process_login") = [](std::map<std::string, std::string>&& query) -> std::string {
    if (query.contains("username") && query.contains("password")) {
      auto& username = query["username"];
      auto& password = query["password"];
      DataBase db{};
      db.Open("chat", "credits");
      if (!db.SelectColumns(std::tuple{"username", "password"},
                            DataBase::Condition().Equal("username", username).And().Equal("password", password))
               .empty()) {
        return "/index.html?username=" + username + "&password=" + password;
      }
      return "Invalid credentials";
    }
    return "Invalid request";
  };

  Router("/process_register") = [](std::map<std::string, std::string>&& query) -> std::string {
    if (query.contains("username") && query.contains("password")) {
      auto& username = query["username"];
      auto& password = query["password"];

      DataBase db{};
      db.Open("chat", "credits");
      if (!db.SelectColumns(std::tuple{"username"}, DataBase::Condition().Equal("username", username)).empty()) {
        return "Invalid request.";
      }
      db.InsertRow({"username", "password"}, username, password);

      return "HTTP/1.1 302 Found\r\nLocation: /login.html\r\n\r\n";
    }
    return "Invalid request.";
  };

  Router("/index.html") = [](std::map<std::string, std::string>&& query) -> std::string {
    if (query.contains("username") && query.contains("password")) {
      auto& username = query["username"];
      auto& password = query["password"];
      DataBase db{};
      db.Open("chat", "credits");
      if (!db.SelectColumns(std::tuple{"username", "password"},
                            DataBase::Condition().Equal("username", username).And().Equal("password", password))
               .empty()) {
        return ReadStaticFile(base_path + "/index.html");
      }
    }
    return "HTTP/1.1 302 Found\r\nLocation: /login.html\r\n\r\n";
  };
  Router("/") = [](std::map<std::string, std::string>&& query) -> std::string {
    return "HTTP/1.1 302 Found\r\nLocation: /login.html\r\n\r\n";
  };
  Socket socket("0.0.0.0", "11234", (std::move(base_path)), SocketType::TCP);
}