#pragma once

#include "sql/operator/logical_operator.h"
#include "sql/expr/expression.h"
#include <vector>
#include <memory>

class SortLogicalOperator : public LogicalOperator
{
public:
  SortLogicalOperator(std::vector<std::unique_ptr<Expression>> order_by_exprs)
      : order_by_(std::move(order_by_exprs))
  {}

  virtual ~SortLogicalOperator() = default;

  LogicalOperatorType type() const override { return LogicalOperatorType::SORT; }

  std::vector<std::unique_ptr<Expression>> &order_by() { return order_by_; }

  int limit() const { return limit_; }
  void set_limit(int limit) { limit_ = limit; }

private:
  std::vector<std::unique_ptr<Expression>> order_by_;
  int limit_ = -1;
};