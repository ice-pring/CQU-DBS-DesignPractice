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

#include "sql/executor/create_index_executor.h"
#include "storage/index/ivfflat_index.h"
#include "common/log/log.h"
#include "event/session_event.h"
#include "event/sql_event.h"
#include "session/session.h"
#include "sql/stmt/create_index_stmt.h"
#include "storage/table/table.h"


RC CreateIndexExecutor::execute(SQLStageEvent *sql_event)
{
  Stmt    *stmt    = sql_event->stmt();
  Session *session = sql_event->session_event()->session();
  ASSERT(stmt->type() == StmtType::CREATE_INDEX,
      "create index executor can not run this command: %d",
      static_cast<int>(stmt->type()));

  CreateIndexStmt *create_index_stmt = static_cast<CreateIndexStmt *>(stmt);

  Trx   *trx   = session->current_trx();
  Table *table = create_index_stmt->table();

  // 新增分支：构建 IVF_Flat 近似索引
  if (create_index_stmt->is_vector()) {
     if (create_index_stmt->lists() <= 0) return RC::INVALID_ARGUMENT;
     IvfflatIndex *vec_idx = new IvfflatIndex(create_index_stmt->lists(), create_index_stmt->probes());
     RC rc = vec_idx->create(table, create_index_stmt->field_meta(), create_index_stmt->index_name().c_str());
     if (rc == RC::SUCCESS) {
        table->add_vector_index(vec_idx);
     } else {
        delete vec_idx;
     }
     return rc;
  }

  return table->create_index(trx, create_index_stmt->field_meta(), create_index_stmt->index_name().c_str());
}