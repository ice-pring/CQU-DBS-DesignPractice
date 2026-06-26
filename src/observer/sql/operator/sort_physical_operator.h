#pragma once

#include "sql/operator/physical_operator.h"
#include "sql/expr/expression.h"
#include "sql/expr/tuple.h"
#include <vector>
#include <memory>

struct SortItem {
  Tuple *tuple;
  std::vector<Value> sort_keys;
};

class SortPhysicalOperator : public PhysicalOperator
{
public:
  SortPhysicalOperator(std::vector<std::unique_ptr<Expression>> order_by);
  virtual ~SortPhysicalOperator();

  // 补齐 Trx* 事务上下文指针
  RC open(Trx *trx) override;
  RC next() override;
  RC close() override;
  Tuple *current_tuple() override;
  PhysicalOperatorType type() const override { return PhysicalOperatorType::SORT; }

private:
  std::vector<std::unique_ptr<Expression>> order_by_;
  std::vector<SortItem> items_;
  size_t current_idx_;
};