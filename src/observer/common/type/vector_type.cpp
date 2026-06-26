#include "common/type/vector_type.h"
#include "common/value.h"
#include "common/log/log.h"
#include <sstream>
#include <string>

int VectorType::compare(const Value &left, const Value &right) const {
  if (left.attr_type() != AttrType::VECTORS || right.attr_type() != AttrType::VECTORS) {
    return INT32_MAX; 
  }
  if (left.length() != right.length()) {
    return INT32_MAX;
  }
  const float *l_data = (const float *)left.data();
  const float *r_data = (const float *)right.data();
  int count = left.length() / sizeof(float);
  for (int i = 0; i < count; ++i) {
    if (l_data[i] < r_data[i]) return -1;
    if (l_data[i] > r_data[i]) return 1;
  }
  return 0;
}

RC VectorType::to_string(const Value &val, std::string &result) const {
  if (val.attr_type() != AttrType::VECTORS) {
    return RC::INVALID_ARGUMENT;
  }
  int count = val.length() / sizeof(float);
  const float *data = (const float *)val.data();
  result = "[";
  for (int i = 0; i < count; ++i) {
    result += std::to_string(data[i]);
    if (i < count - 1) result += ",";
  }
  result += "]";
  return RC::SUCCESS;
}

RC VectorType::string_to_vector(const char *str, std::vector<float> &vec) {
  std::string s = str;
  size_t start = s.find('[');
  size_t end = s.rfind(']');
  if (start == std::string::npos || end == std::string::npos || start >= end) {
    return RC::INVALID_ARGUMENT;
  }
  std::string content = s.substr(start + 1, end - start - 1);
  std::stringstream ss(content);
  std::string item;
  while (std::getline(ss, item, ',')) {
    size_t first = item.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) continue;
    size_t last = item.find_last_not_of(" \t\r\n");
    std::string num_str = item.substr(first, last - first + 1);
    try {
      size_t pos;
      float val = std::stof(num_str, &pos);
      if (pos != num_str.length()) {
        return RC::INVALID_ARGUMENT;
      }
      vec.push_back(val);
    } catch (...) {
      return RC::INVALID_ARGUMENT;
    }
  }
  if (vec.empty()) return RC::INVALID_ARGUMENT;
  return RC::SUCCESS;
}