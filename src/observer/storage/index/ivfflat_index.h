#pragma once

#include "storage/index/index.h"
#include "common/value.h"
#include "storage/table/table.h"
#include "storage/record/record.h"
#include <vector>
#include <string>

struct VectorEntry {
  std::vector<float> vec;
  RID rid;
};

struct Centroid {
  std::vector<float> center;
  std::vector<VectorEntry> entries;
};

class IvfflatIndex : public Index
{
public:
  IvfflatIndex(int lists, int probes) : lists_(lists), probes_(probes) {}
  virtual ~IvfflatIndex() noexcept = default;

  // 自定义初始化入口
  RC create(Table *table, const FieldMeta *field, const char *index_name);
  
  // 提供给表的记录插入接口，用于增量构建聚类
  void insert_record(Record &record);

  // 近似搜索查询核心入口
  std::vector<RID> ann_search(const Value &query, int top_k);
  
  const char* field() const { return field_name_.c_str(); }

  RC create(Table *table, const char *file_name, const IndexMeta &index_meta, const FieldMeta &field_meta) { return RC::SUCCESS; }
  RC open(Table *table, const char *file_name, const IndexMeta &index_meta, const FieldMeta &field_meta) { return RC::SUCCESS; }
  RC close() { return RC::SUCCESS; }
  RC insert_entry(const char *record, const RID *rid) override { return RC::SUCCESS; }
  RC delete_entry(const char *record, const RID *rid) override { return RC::SUCCESS; }
  RC sync() override { return RC::SUCCESS; }
  IndexScanner *create_scanner(const char *left_key, int left_len, bool left_inclusive, const char *right_key, int right_len, bool right_inclusive) override { return nullptr; }

private:
  int lists_;
  int probes_;
  std::string field_name_;
  std::vector<Centroid> centroids_;
  const FieldMeta *field_meta_ = nullptr;
  
  float calc_l2(const std::vector<float> &v1, const std::vector<float> &v2) const;
};