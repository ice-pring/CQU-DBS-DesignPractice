#include "storage/index/ivfflat_index.h"
#include "storage/record/record_scanner.h"
#include <cmath>
#include <algorithm>

float IvfflatIndex::calc_l2(const std::vector<float> &v1, const std::vector<float> &v2) const {
   float dist = 0;
   for (size_t i=0; i<v1.size(); ++i) {
      float d = v1[i] - v2[i];
      dist += d * d;
   }
   return dist;
}

RC IvfflatIndex::create(Table *table, const FieldMeta *field, const char *index_name) {
   field_name_ = field->name();
   field_meta_ = field;
   
   std::vector<VectorEntry> all_entries;
   RecordScanner *scanner = nullptr;
   table->get_record_scanner(scanner, nullptr, ReadWriteMode::READ_ONLY);
   if (scanner) {
       Record rec;
       // 【修改点】：修复 scanner 迭代逻辑，适配 MiniOB 原生接口
       while (scanner->next(rec) == RC::SUCCESS) {
           const char *data = rec.data() + field->offset();
           int dim = field->len() / sizeof(float);
           std::vector<float> vec((const float*)data, (const float*)data + dim);
           all_entries.push_back({vec, rec.rid()});
       }
       delete scanner;
   }
   
   if (all_entries.empty()) return RC::SUCCESS;

   int actual_lists = std::min((int)all_entries.size(), lists_);
   centroids_.resize(actual_lists);
   for (int i=0; i<actual_lists; ++i) {
       centroids_[i].center = all_entries[i].vec;
   }

   // 简易稳定版 K-Means 迭代 5 次划分 Voronoi 多边形空间
   for (int iter=0; iter<5; ++iter) {
       for (auto &c : centroids_) c.entries.clear();
       for (const auto &entry : all_entries) {
           int best_c = 0;
           float min_d = 1e9;
           for (int i=0; i<actual_lists; ++i) {
               float d = calc_l2(entry.vec, centroids_[i].center);
               if (d < min_d) { min_d = d; best_c = i; }
           }
           centroids_[best_c].entries.push_back(entry);
       }
       for (auto &c : centroids_) {
           if (c.entries.empty()) continue;
           int dim = c.center.size();
           std::vector<float> new_center(dim, 0.0f);
           for (const auto &e : c.entries) {
               for(int i=0; i<dim; ++i) new_center[i] += e.vec[i];
           }
           for(int i=0; i<dim; ++i) c.center[i] = new_center[i] / c.entries.size();
       }
   }
   return RC::SUCCESS;
}

void IvfflatIndex::insert_record(Record &record) {
   if (centroids_.empty() || !field_meta_) return;
   const char *data = record.data() + field_meta_->offset();
   int dim = field_meta_->len() / sizeof(float);
   std::vector<float> vec((const float*)data, (const float*)data + dim);
   
   int best_c = 0;
   float min_d = 1e9;
   for (size_t i=0; i<centroids_.size(); ++i) {
       float d = calc_l2(vec, centroids_[i].center);
       if (d < min_d) { min_d = d; best_c = i; }
   }
   centroids_[best_c].entries.push_back({vec, record.rid()});
}

std::vector<RID> IvfflatIndex::ann_search(const Value &query, int top_k) {
   std::vector<RID> res;
   if (centroids_.empty() || query.attr_type() != AttrType::VECTORS) return res;
   
   int dim = query.length() / sizeof(float);
   std::vector<float> q_vec((const float*)query.data(), (const float*)query.data() + dim);

   std::vector<std::pair<float, int>> c_dists;
   for (size_t i=0; i<centroids_.size(); ++i) {
       c_dists.push_back({calc_l2(q_vec, centroids_[i].center), (int)i});
   }
   std::sort(c_dists.begin(), c_dists.end()); // 寻找最近的若干个聚类中心
   
   int p = std::min(probes_, (int)centroids_.size());
   std::vector<std::pair<float, RID>> candidates;
   for (int i=0; i<p; ++i) {
       int c_idx = c_dists[i].second;
       for (const auto &e : centroids_[c_idx].entries) {
           candidates.push_back({calc_l2(q_vec, e.vec), e.rid});
       }
   }
   
   // 从选定的候选集中进行Top-K排序并截断返回
   std::sort(candidates.begin(), candidates.end(), [](auto &a, auto &b){ return a.first < b.first; });
   int k = std::min(top_k, (int)candidates.size());
   for (int i=0; i<k; ++i) res.push_back(candidates[i].second);
   return res;
}