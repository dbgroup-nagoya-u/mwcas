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
// #include "dbgroup/atomic/mwcas/lock_free/mwcas_descriptor.hpp"
#include "/home/unno/sotsuron/mwcas/include/dbgroup/atomic/mwcas/lock_free/mwcas_descriptor.hpp"

// C++ standard libraries
#include <atomic>
#include <cstddef>

// external libraries
// #include "dbgroup/lock/common.hpp"
#include "/home/unno/sotsuron/mwcas/test/common.hpp"

// local sources
// #include "dbgroup/atomic/mwcas/utility.hpp"
#include "/home/unno/sotsuron/mwcas/include/dbgroup/atomic/mwcas/utility.hpp"

namespace dbgroup::atomic::mwcas::lock_free
{

auto
MwCASDescriptor::MwCAS()  //
    -> bool
{
  if (stat_.load(kRelaxed) == kUndecided) {
    auto status = kSucceeded;
    const auto desc_addr = std::bit_cast<uint64_t>(this) | kMwCASFlag;  // この位置でいいのか疑問
    for (size_t i = 0; i < target_cnt_; ++i) {
      auto &target = targets_[i];
      auto word = target.addr->compare_exchange_strong(target.old_val, desc_addr, kRelaxed);
      while ((word & kMwCASFlag > 0) && word != desc_addr) {
        auto another_desc_addr = std::move(word);
        std::this_thread::sleep_for(kBackOffTime);
        auto word = target.addr->compare_exchange_strong(target.old_val, desc_addr, kRelaxed);
        if (word == another_desc_addr) {
          auto *another_desc = std::bit_cast<MwCASDescriptor *>(another_desc_addr & ~kMwCASFlag);
          another_desc->MwCAS();
          auto word = target.addr->compare_exchange_strong(target.old_val, desc_addr, kRelaxed);
        }
      }
      if (word != (std::bit_cast<uint64_t>(this) | kMwCASFlag) && word != target.old_val) {
        status = kFailed;
        break;
      }
    }
    stat_.compare_exchange_strong(kUndecided, status, kRelaxed);
  }
  auto is_succeeded = (stat_.load(kRelaxed) == kSucceeded);
  for (size_t i = 0; i < target_cnt_; ++i) {
    auto &target = targets_[i];
    if (is_succeeded) {
      target.addr->compare_exchange_strong(std::bit_cast<uint64_t>(this) | kMwCASFlag,
                                           target.new_val, kRelaxed);
    } else {
      target.addr->compare_exchange_strong(std::bit_cast<uint64_t>(this) | kMwCASFlag,
                                           target.old_val, kRelaxed);
    }
  }

  return is_succeeded;
}

}  // namespace dbgroup::atomic::mwcas::lock_free
