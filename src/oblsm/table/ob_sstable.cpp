/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "oblsm/table/ob_sstable.h"
#include "oblsm/util/ob_coding.h"
#include "common/log/log.h"
#include "common/lang/filesystem.h"

namespace oceanbase {

void ObSSTable::init()
{
  // your code here
  file_reader_ = ObFileReader::create_file_reader(file_name_);
  
  file_reader_->open_file();
  uint32_t size = file_reader_->file_size();
  // read meta-size pos
  string meta_size_pos_str = file_reader_->read_pos(size - sizeof(uint32_t), sizeof(uint32_t));
  uint32_t meta_size_pos = get_numeric<uint32_t>(meta_size_pos_str.c_str());
  // read meta size
  string meta_size_str = file_reader_->read_pos(meta_size_pos, sizeof(uint32_t));
  uint32_t meta_size = get_numeric<uint32_t>(meta_size_str.c_str());

  // start to read block meta
  uint32_t meta_start_pos = meta_size_pos + sizeof(uint32_t);
  //std::cout << "init process:" << meta_size << std::endl;
  for(int i = 0; i < meta_size; i++) {
    // read block meta size 
    string block_meta_size_str = file_reader_->read_pos(meta_start_pos, sizeof(uint32_t));
    uint32_t block_meta_size = get_numeric<uint32_t>(block_meta_size_str.c_str());
    //std::cout << "block_meta_size::" << block_meta_size << std::endl;
    // read block meta
    string block_meta = file_reader_->read_pos(meta_start_pos + sizeof(uint32_t), block_meta_size);
    BlockMeta bm;
    bm.decode(block_meta);
    block_metas_.push_back(bm);

    meta_start_pos += sizeof(uint32_t) + block_meta_size;
  }
  // close ?
  //file_reader_->close_file();
}

shared_ptr<ObBlock> ObSSTable::read_block_with_cache(uint32_t block_idx) const
{
  // your code here
  // tudo
  return read_block(block_idx);
}

shared_ptr<ObBlock> ObSSTable::read_block(uint32_t block_idx) const
{
  // your code here
  const BlockMeta bm = block_meta(block_idx);
  uint32_t offset = bm.offset_;
  uint32_t size = bm.size_;
  string block_str = file_reader_->read_pos(offset, size); 
  const ObComparator * ob_comparator = this->comparator();
  ObBlock ob_block(ob_comparator);
  ob_block.decode(block_str);
  return std::make_shared<ObBlock>(ob_block);
}

void ObSSTable::remove() { filesystem::remove(file_name_); }

ObLsmIterator *ObSSTable::new_iterator() { return new TableIterator(get_shared_ptr()); }

void TableIterator::read_block_with_cache()
{
  block_ = sst_->read_block_with_cache(curr_block_idx_);
  block_iterator_.reset(block_->new_iterator());
}

void TableIterator::seek_to_first()
{
  curr_block_idx_ = 0;
  read_block_with_cache();
  block_iterator_->seek_to_first();
}

void TableIterator::seek_to_last()
{
  curr_block_idx_ = block_cnt_ - 1;
  read_block_with_cache();
  block_iterator_->seek_to_last();
}

void TableIterator::next()
{
  block_iterator_->next();
  if (block_iterator_->valid()) {
  } else if (curr_block_idx_ < block_cnt_ - 1) {
    curr_block_idx_++;
    read_block_with_cache();
    block_iterator_->seek_to_first();
  }
}

void TableIterator::seek(const string_view &lookup_key)
{
  curr_block_idx_ = 0;
  // TODO: use binary search
  for (; curr_block_idx_ < block_cnt_; curr_block_idx_++) {
    const auto &block_meta = sst_->block_meta(curr_block_idx_);
    if (sst_->comparator()->compare(extract_user_key(block_meta.last_key_), extract_user_key_from_lookup_key(lookup_key)) >= 0) {
      break;
    }
  }
  if (curr_block_idx_ == block_cnt_) {
    block_iterator_ = nullptr;
    return;
  }
  read_block_with_cache();
  block_iterator_->seek(lookup_key);
};

}  // namespace oceanbase
