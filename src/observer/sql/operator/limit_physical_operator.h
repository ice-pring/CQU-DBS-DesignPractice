#pragma once
#include "sql/operator/physical_operator.h"

class LimitPhysicalOperator : public PhysicalOperator {
public:
   LimitPhysicalOperator(int limit) : limit_(limit), count_(0) {}
   RC open(Trx *trx) override {
       count_ = 0;
       if (children_.empty()) return RC::INTERNAL;
       return children_[0]->open(trx);
   }
   RC next() override {
       if (limit_ >= 0 && count_ >= limit_) return RC::RECORD_EOF;
       RC rc = children_[0]->next();
       if (rc == RC::SUCCESS) count_++;
       return rc;
   }
   RC close() override {
       if (children_.empty()) return RC::SUCCESS;
       return children_[0]->close();
   }
   Tuple *current_tuple() override {
       return children_[0]->current_tuple();
   }
   PhysicalOperatorType type() const override { return PhysicalOperatorType::LIMIT; }
private:
   int limit_;
   int count_;
};