#pragma once

#include "common/type/data_type.h"
#include <vector>
#include <string>

class VectorType : public DataType
{
public:
  VectorType() : DataType(AttrType::VECTORS) {}
  virtual ~VectorType() {}

  int compare(const Value &left, const Value &right) const override;

  RC add(const Value &left, const Value &right, Value &result) const override { return RC::UNIMPLEMENTED; }
  RC subtract(const Value &left, const Value &right, Value &result) const override { return RC::UNIMPLEMENTED; }
  RC multiply(const Value &left, const Value &right, Value &result) const override { return RC::UNIMPLEMENTED; }

  RC to_string(const Value &val, std::string &result) const override;
  
  static RC string_to_vector(const char *str, std::vector<float> &vec);
};