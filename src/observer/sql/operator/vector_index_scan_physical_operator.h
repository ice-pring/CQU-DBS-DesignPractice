#pragma once
#include "sql/operator/physical_operator.h"
#include "storage/index/ivfflat_index.h"
#include "storage/table/table.h"
#include "storage/table/table_meta.h"
#include "storage/field/field_meta.h"
#include <vector>

class VectorIndexScanPhysicalOperator : public PhysicalOperator {
public:
   VectorIndexScanPhysicalOperator(Table *table, IvfflatIndex *index, Value query, int top_k)
      : table_(table), index_(index), query_(query), top_k_(top_k) {
      
      // 提取表的所有列元数据，以供 RowTuple 进行正确解析
      const TableMeta &meta = table_->table_meta();
      for (int i = 0; i < meta.field_num(); ++i) {
          fields_.push_back(*meta.field(i));
      }
      tuple_.set_schema(table_, &fields_);
   }
   
   RC open(Trx *trx) override {
       rids_ = index_->ann_search(query_, top_k_);
       idx_ = 0;
       return RC::SUCCESS;
   }
   
   RC next() override {
       if (idx_ < rids_.size()) {
           RC rc = table_->get_record(rids_[idx_], record_);
           if (rc == RC::SUCCESS) {
               tuple_.set_record(&record_);
               idx_++;
               return RC::SUCCESS;
           }
           return rc;
       }
       return RC::RECORD_EOF;
   }
   
   RC close() override { return RC::SUCCESS; }
   
   Tuple *current_tuple() override { return &tuple_; }
   
   PhysicalOperatorType type() const override { return PhysicalOperatorType::VECTOR_INDEX_SCAN; }
   
   string name() const override { return "VECTOR_INDEX_SCAN"; }

private:
   Table *table_;
   IvfflatIndex *index_;
   Value query_;
   int top_k_;
   std::vector<RID> rids_;
   size_t idx_ = 0;
   Record record_;
   std::vector<FieldMeta> fields_;
   RowTuple tuple_;
};