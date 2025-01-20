/*
 * Copyright 2024 Database Group, Nagoya University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// the corresponding header
#include "dbgroup/atomic/mwcas/lock_free/mwcas_descriptor.hpp"

// C++ standard libraries
#include <atomic>
#include <cstddef>

// external libraries
#include "dbgroup/lock/common.hpp"

// local sources
#include "dbgroup/atomic/mwcas/utility.hpp"

namespace dbgroup::atomic::mwcas::lock_free
{

auto
MwCASDescriptor::MwCAS()  //
    -> bool
{
  const auto desc_addr = std::bit_cast<uint64_t>(this) | kMwCASFlag;
  auto cur_stat = stat_.load(kSeqCst);
  if (cur_stat == kUndecided) {
    auto stat = kSucceeded;
    for (size_t i = 0; i < target_cnt_; ++i) {
      auto &target = targets_[i];
      auto word = target.addr->compare_exchange_strong(target.old_val, desc_addr, target.fence,
                                                       target.fence);
      while (((word & kMwCASFlag) > 0) && word != desc_addr) {
        auto another_desc_addr = word;
        std::this_thread::sleep_for(kBackOffTime);
        word = target.addr->compare_exchange_strong(target.old_val, desc_addr, target.fence,
                                                    target.fence);
        if (word == another_desc_addr) {
          auto *another_desc = std::bit_cast<MwCASDescriptor *>(another_desc_addr & ~kMwCASFlag);
          another_desc->MwCAS();
          word = target.addr->compare_exchange_strong(target.old_val, desc_addr, target.fence,
                                                      target.fence);
        }
      }
      if (word != (desc_addr) && word != target.old_val) {
        stat = kFailed;
        break;
      }
    }
    cur_stat = stat_.load(kSeqCst);
    if (cur_stat == kUndecided) {
      stat_.compare_exchange_strong(cur_stat, stat, kSeqCst, kSeqCst);
    }
    cur_stat = stat;
  }
  auto is_succeeded = (stat_.load(std::memory_order_seq_cst) == kSucceeded);
  auto expected = desc_addr;  // あまりよくない方法かも
  for (size_t i = 0; i < target_cnt_; ++i) {
    auto &target = targets_[i];
    if (is_succeeded) {
      target.addr->compare_exchange_strong(expected, target.new_val, target.fence, target.fence);
    } else {
      target.addr->compare_exchange_strong(expected, target.old_val, target.fence, target.fence);
    }
  }

  return is_succeeded;
}

}  // namespace dbgroup::atomic::mwcas::lock_free
