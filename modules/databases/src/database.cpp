//
// Created by lhy on 24-9-24.
//

#include "web/databases/database.h"
namespace lhy {
void DataBase::Open(const std::string& schema, const std::string& table) {
  schema_ = session_.getSchema(schema);
  table_ = schema_->getTable(table);
}
DataBase::DataBase() : session_(client_.getSession()) {}

}  // namespace lhy