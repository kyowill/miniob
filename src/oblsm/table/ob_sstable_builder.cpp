/* Copyright (c) 2021 OceanBase and/or its affiliates. All rights reserved.
miniob is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
         http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "oblsm/table/ob_sstable_builder.h"
#include "oblsm/util/ob_coding.h"

namespace oceanbase {

// TODO: refactor build with mem_table/iterator logic.
RC ObSSTableBuilder::build(shared_ptr<ObMemTable> mem_table, const std::string &file_name, uint32_t sst_id)
{
  //init memtable iterator
  // ObMemTableIterator iter(mem_table, mem_table.get());
  ObLsmIterator* iter = mem_table->new_iterator();
  file_writer_ = ObFileWriter::create_file_writer(file_name, false);
  file_writer_->open_file();
  iter->seek_to_first();// move to first
  while(iter->valid()) {
    string_view key = iter->key();
    string_view val = iter->value();
    if (curr_blk_first_key_.size() == 0) {
      curr_blk_first_key_.assign(key);
    }
    RC rc = block_builder_.add(key, val);
    if (rc != RC::SUCCESS) {
      // block is full, flush to disc
      // construct block meta
      finish_build_block();
      // clear first key
      curr_blk_first_key_.clear();
      block_builder_.add(key, val);
    }
    iter->next();
  }
  //std::cout << "size:" << block_builder_.appro_size() << std::endl;
  // flush last block
  if (block_builder_.appro_size() != 0) {
    finish_build_block();
  }
  // flush block metas
  // append size 
  string metas_str;
  put_numeric<uint32_t>(&metas_str, block_metas_.size());

  for(int i = 0; i < block_metas_.size(); i++) {
    // append ith size
    uint32_t meta_size = 4 * sizeof(uint32_t) + block_metas_[i].first_key_.size() + block_metas_[i].last_key_.size();
    put_numeric<uint32_t>(&metas_str, meta_size);
    // append block meta
    string meta_str = block_metas_[i].encode();
    //std::cout << "build process:" << meta_str.size() << std::endl;
    metas_str.append(meta_str);
  }
  // append meta start offset
  put_numeric<uint32_t>(&metas_str, curr_offset_);
  file_writer_->write(metas_str);
  file_writer_->flush();
  return RC::SUCCESS;;
}

void ObSSTableBuilder::finish_build_block()
{
  string      last_key       = block_builder_.last_key();
  string_view block_contents = block_builder_.finish();
  file_writer_->write(block_contents);
  block_metas_.push_back(BlockMeta(curr_blk_first_key_, last_key, curr_offset_, block_contents.size()));
  // TODO: block aligned to BLOCK_SIZE
  curr_offset_ += block_contents.size();
  block_builder_.reset();
}

shared_ptr<ObSSTable> ObSSTableBuilder::get_built_table()
{
  // TODO: sstable should have more metadata
  shared_ptr<ObSSTable> sstable = make_shared<ObSSTable>(sst_id_, file_writer_->file_name(), comparator_, block_cache_);
  sstable->init();
  return sstable;
}

void ObSSTableBuilder::reset()
{
  block_builder_.reset();
  curr_blk_first_key_.clear();
  if (file_writer_ != nullptr) {
    file_writer_.reset(nullptr);
  }
  block_metas_.clear();
  curr_offset_ = 0;
  sst_id_      = 0;
  file_size_   = 0;
}
}  // namespace oceanbase
