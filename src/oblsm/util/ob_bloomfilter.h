/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#pragma once

#include "common/lang/string.h"
#include <atomic>
#include <vector> 

namespace oceanbase {

/**
 * @class ObBloomfilter
 * @brief A simple Bloom filter implementation(Need to support concurrency).
 */
class ObBloomfilter
{
public:
  /**
   * @brief Constructs a Bloom filter with specified parameters.
   *
   * @param hash_func_count Number of hash functions to use. Default is 4.
   * @param total_bits Total number of bits in the Bloom filter. Default is 65536.
   */
  ObBloomfilter(size_t hash_func_count = 4, size_t total_bits = 65536) : hash_func_count_(hash_func_count), total_bits_(total_bits), bitset_(total_bits), counter_(0) {

  }

  /**
   * @brief Inserts an object into the Bloom filter.
   * @details This method computes hash values for the given object and sets corresponding bits in the filter.
   * @param object The object to be inserted.
   */
  void insert(const string &object) {
    uint64_t h1 = murmurhash(object);
    uint64_t h2 = h1 >> 32;
    for (size_t i = 0; i < hash_func_count_; i++) {
        size_t pos = (h1 + i * h2) % total_bits_;
        // thread safe
        bitset_[pos].store(true);
    }
    counter_.fetch_add(1);
  }

  /**
   * @brief Clears all entries in the Bloom filter.
   *
   * @details Resets the filter, removing all previously inserted objects.
   */
  void clear() {
    counter_.store(0);
    for(int i = 0; i < total_bits_; i++) {
      bitset_[i].store(false);
    }
  }

  /**
   * @brief Checks if an object is possibly in the Bloom filter.
   *
   * @param object The object to be checked.
   * @return true if the object might be in the filter, false if definitely not.
   */
  bool contains(const string &object) const { 
    uint64_t h1 = murmurhash(object);
    uint64_t h2 = h1 >> 32;
    bool flag = true;
    for (size_t i = 0; i < hash_func_count_; i++) {
        size_t pos = (h1 + i * h2) % total_bits_;
        // thread safe
        flag = flag && bitset_[pos].load();
        if (!flag) {
          return false;
        }
    }
    return true; 
  }

  /**
   * @brief Returns the count of objects inserted into the Bloom filter.
   */
  size_t object_count() const { 
    return counter_.load();
  }

  /**
   * @brief Checks if the Bloom filter is empty.
   * @return true if the filter is empty, false otherwise.
   */
  bool empty() const { return 0 == object_count(); }

private:
  size_t hash_func_count_;
  size_t total_bits_;
  std::vector<std::atomic<bool>> bitset_;
  std::atomic<size_t> counter_;
  static uint64_t murmurhash(const std::string& key) {
    uint64_t hash = 0;
    for (char c : key) {
        hash ^= c;
        hash *= 0x5bd1e995;
        hash ^= hash >> 15;
    }
    return hash;
  }
};

}  // namespace oceanbase
