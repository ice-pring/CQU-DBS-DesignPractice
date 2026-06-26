/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

//
// Created by Wangyunlai on 2023/4/25.
//

#pragma once

#include "sql/stmt/stmt.h"

struct CreateIndexSqlNode;
class Table;
class FieldMeta;

/**
 * @brief 创建索引的语句
 * @ingroup Statement
 */
class CreateIndexStmt : public Stmt
{
public:
  CreateIndexStmt(Table *table, const FieldMeta *field_meta, const string &index_name)
      : table_(table), field_meta_(field_meta), index_name_(index_name)
  {}

  virtual ~CreateIndexStmt() = default;

  StmtType type() const override { return StmtType::CREATE_INDEX; }

  Table           *table() const { return table_; }
  const FieldMeta *field_meta() const { return field_meta_; }
  const string    &index_name() const { return index_name_; }

public:
  static RC create(Db *db, const CreateIndexSqlNode &create_index, Stmt *&stmt);

  // 新增接口
  bool is_vector() const { return is_vector_; }
  int lists() const { return lists_; }
  int probes() const { return probes_; }
  void set_vector_params(bool is_vec, int lists, int probes) {
     is_vector_ = is_vec; lists_ = lists; probes_ = probes;
  }

private:
  Table           *table_      = nullptr;
  const FieldMeta *field_meta_ = nullptr;
  string           index_name_;

  // 新增字段
  bool is_vector_ = false;
  int lists_ = 245;
  int probes_ = 5;
};
