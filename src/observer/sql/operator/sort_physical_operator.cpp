#include "sql/operator/sort_physical_operator.h"
#include "common/log/log.h"
#include <algorithm>

SortPhysicalOperator::SortPhysicalOperator(std::vector<std::unique_ptr<Expression>> order_by)
    : order_by_(std::move(order_by)), current_idx_(0)
{}

SortPhysicalOperator::~SortPhysicalOperator()
{
  for (auto &item : items_) {
    delete item.tuple;
  }
}

// 接收 trx 并在下发时传递
RC SortPhysicalOperator::open(Trx *trx)
{
  RC rc = RC::SUCCESS;
  if (children_.size() != 1) {
    LOG_WARN("Sort operator should have exactly 1 child");
    return RC::INTERNAL;
  }
  
  PhysicalOperator *child = children_[0].get();
  rc = child->open(trx);
  if (rc != RC::SUCCESS) {
    return rc;
  }

  items_.clear();
  current_idx_ = 0;

  while (true) {
    rc = child->next();
    if (rc == RC::RECORD_EOF) {
      rc = RC::SUCCESS;
      break;
    } else if (rc != RC::SUCCESS) {
      return rc;
    }

    Tuple *child_tuple = child->current_tuple();
    if (child_tuple == nullptr) {
      return RC::INTERNAL;
    }

    // 利用 ValueListTuple::make 实现完美深拷贝物化
    ValueListTuple *cloned_tuple = new ValueListTuple();
    rc = ValueListTuple::make(*child_tuple, *cloned_tuple);
    if (rc != RC::SUCCESS) {
      delete cloned_tuple;
      for (auto &it : items_) delete it.tuple;
      items_.clear();
      return rc;
    }

    SortItem item;
    item.tuple = cloned_tuple;
    
    // 计算 order_by 键值（基于已深拷贝的元组，确保安全）
    for (auto &expr : order_by_) {
      Value key_val;
      RC eval_rc = expr->get_value(*cloned_tuple, key_val);
      if (eval_rc != RC::SUCCESS) {
        delete cloned_tuple;
        for (auto &it : items_) delete it.tuple;
        items_.clear();
        return eval_rc;
      }
      item.sort_keys.push_back(key_val);
    }
    
    items_.push_back(std::move(item));
  }

  // 升序排序逻辑
  std::sort(items_.begin(), items_.end(), [](const SortItem &a, const SortItem &b) {
    for (size_t i = 0; i < a.sort_keys.size(); ++i) {
      int cmp = a.sort_keys[i].compare(b.sort_keys[i]);
      if (cmp < 0) return true;
      if (cmp > 0) return false;
    }
    return false;
  });

  return RC::SUCCESS;
}

RC SortPhysicalOperator::next()
{
  if (current_idx_ < items_.size()) {
    current_idx_++;
    return RC::SUCCESS;
  }
  return RC::RECORD_EOF;
}

RC SortPhysicalOperator::close()
{
  for (auto &item : items_) {
    delete item.tuple;
  }
  items_.clear();
  current_idx_ = 0;
  if (children_.size() == 1) {
    return children_[0]->close();
  }
  return RC::SUCCESS;
}

Tuple *SortPhysicalOperator::current_tuple()
{
  if (current_idx_ > 0 && current_idx_ <= items_.size()) {
    return items_[current_idx_ - 1].tuple;
  }
  return nullptr;
}