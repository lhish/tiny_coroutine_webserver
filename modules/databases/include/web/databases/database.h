//
// Created by lhy on 24-9-24.
//

#ifndef DATABASE_H
#define DATABASE_H

#include <mysqlx/xdevapi.h>

#include <cctype>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>

namespace lhy {

struct DbConfig {
  std::string username;
  std::string password;
  std::string host;
  int port;
};

namespace {

std::string extract_json_value(const std::string& json_str, const std::string& key) {
  std::string search_key = "\"" + key + "\":";
  size_t key_pos = json_str.find(search_key);
  if (key_pos == std::string::npos) {
    return "";
  }
  size_t value_start_pos = key_pos + search_key.length();

  while (value_start_pos < json_str.length() && isspace(json_str[value_start_pos])) {
    value_start_pos++;
  }

  if (json_str[value_start_pos] == '"') {
    size_t value_start = value_start_pos + 1;
    size_t value_end = json_str.find("\"", value_start);
    if (value_end == std::string::npos) {
      return "";
    }
    return json_str.substr(value_start, value_end - value_start);
  }
  size_t value_end = value_start_pos;
  while (value_end < json_str.length() &&
         (isalnum(json_str[value_end]) || json_str[value_end] == '.' || json_str[value_end] == '-' ||
          (json_str[value_end] >= '0' && json_str[value_end] <= '9'))) {
    value_end++;
  }
  return json_str.substr(value_start_pos, value_end - value_start_pos);
}

DbConfig load_db_config() {
  DbConfig config;
  std::ifstream config_file("../database_config.json");
  std::stringstream buffer;
  buffer << config_file.rdbuf();
  std::string json_content = buffer.str();

  config.username = extract_json_value(json_content, "username");
  config.password = extract_json_value(json_content, "password");
  config.host = extract_json_value(json_content, "host");
  std::string port_str = extract_json_value(json_content, "port");
  try {
    config.port = std::stoi(port_str);
  } catch (const std::invalid_argument& e) {
    config.port = 33060;
  } catch (const std::out_of_range& e) {
    config.port = 33060;
  }
  config_file.close();
  return config;
}
}

class DataBase {
 public:
  class Condition {
   public:
    Condition& Equal(const std::string& column, const std::string& value) {
      conditions_.push_back(column + " = '" + value + "'");
      return *this;
    }

    Condition& GreaterThan(const std::string& column, const std::string& value) {
      conditions_.push_back(column + " > '" + value + "'");
      return *this;
    }

    Condition& LessThan(const std::string& column, const std::string& value) {
      conditions_.push_back(column + " < '" + value + "'");
      return *this;
    }

    Condition& And() {
      conditions_.emplace_back(" AND ");
      return *this;
    }

    Condition& Or() {
      conditions_.emplace_back(" OR ");
      return *this;
    }

    [[nodiscard]] std::string ToString() const {
      std::string result;
      for (const auto& condition : conditions_) {
        result += condition;
      }
      return result;
    }

   private:
    std::vector<std::string> conditions_;
  };
  class MysqlRow {
   public:
    MysqlRow(mysqlx::Row row, const std::vector<std::string>& columns) {
      for (size_t i = 0; i < row.colCount(); i++) {
        row_[columns[i]] = row[i];
      }
    }
    mysqlx::Value operator[](const std::string& column) const { return row_.at(column); }

   private:
    std::map<std::string, mysqlx::Value> row_;
  };
  static std::string str(auto value) {
    std::stringstream ss;
    ss << value;
    return ss.str();
  }
  DataBase();
  void Open(const std::string& schema, const std::string& table);
  DataBase& operator=(DataBase&& other) = delete;
  ~DataBase() = default;
  template <typename... Args>
  std::vector<MysqlRow> SelectColumns(const std::tuple<Args...>& columns, const Condition& condition = Condition());
  template <typename... ArgsValues>
  bool InsertRow(const std::vector<std::string>& columns, ArgsValues&&... values);

 private:
  // Load config once
  inline static const DbConfig db_config_ = load_db_config();
  inline static mysqlx::Client client_{
      db_config_.username + ":" + db_config_.password + "@" + db_config_.host + ":" + std::to_string(db_config_.port),
      mysqlx::ClientOption::POOL_MAX_SIZE, 7};
  mysqlx::Session session_;
  std::optional<mysqlx::Schema> schema_;
  std::optional<mysqlx::Table> table_;
  // inline static std::vector<std::wstring> ORDERSTRING = {L"Hero ASC", L"Mana ASC", L"ID ASC"};
};
template <typename... ArgsValues>
bool DataBase::InsertRow(const std::vector<std::string>& columns, ArgsValues&&... values) {
  // Ensure that the number of columns matches the number of values at compile-time
  assert(sizeof...(values) == columns.size());
  // Create an insert query
  auto insert_query = table_->insert(columns);

  // Insert the values
  insert_query.values(std::forward<std::string>(str(values))...).execute();

  return true;
}
template <typename... Args>
std::vector<DataBase::MysqlRow> DataBase::SelectColumns(const std::tuple<Args...>& columns,
                                                        const Condition& condition) {
  mysqlx::RowResult result;

  auto columns_vector = std::apply(
      [&](auto... args) -> std::vector<std::string> {
        if (!condition.ToString().empty()) {
          result = table_->select(args...).where(condition.ToString()).execute();
        } else {
          result = table_->select(args...).execute();
        }
        return {args...};
      },
      columns);

  std::vector<MysqlRow> rows;
  for (auto row : result) {
    rows.emplace_back(row, columns_vector);
  }
  return rows;
}
}  // namespace lhy

#endif  // DATABASE_H
