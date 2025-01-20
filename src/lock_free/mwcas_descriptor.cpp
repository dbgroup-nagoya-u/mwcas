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
/*##############################################################################
 * Static utilities
 *############################################################################*/

auto
MwCASDescriptor::GetDescriptor()  //
    -> MwCASDescriptor *
{
  /*auto *page = _gc->GetPageIfPossible<MwCASDescriptor>();
  auto *desc = (page == nullptr) ? new MwCASDescriptor{} : static_cast<MwCASDescriptor *>(page);
  desc->target_cnt_ = 0;*/
  // GCを無視した一時的な実装
  return new MwCASDescriptor{};
}

/*##############################################################################
 * Utilities
 *############################################################################*/
auto
MwCASDescriptor::MwCAS()  //
    -> bool
{
  // set a memory fence
  stat_.store(kUndecided, kRelease);
  const auto succeeded = MwCASInternal();
  return succeeded;
}

auto
MwCASDescriptor::MwCASInternal(  //
    const size_t begin_pos)      //
    -> bool
{
  const auto desc_addr = std::bit_cast<uint64_t>(this) | kMwCASFlag;
  auto cur_stat = stat_.load(kSeqCst);
  if (cur_stat == kUndecided) {
    auto stat = kSucceeded;
    for (size_t i = begin_pos; i < target_cnt_; ++i) {
      auto &target = targets_[i];
      auto *addr = target.addr;
      auto word = addr->load(kSeqCst);
      while (((word & kMwCASFlag) > 0) && word != desc_addr) {
        const auto another_desc_addr = word;
        std::this_thread::sleep_for(kBackOffTime);
        word = addr->load(kSeqCst);
        if (word != another_desc_addr) continue;  // other threads modified this field

        // a long CPU stall has been detected, so perform another MwCAS
        auto *another_desc = std::bit_cast<MwCASDescriptor *>(another_desc_addr ^ kMwCASFlag);
        another_desc->MwCASInternal();
        word = addr->load(kSeqCst);
      }
      if (word != desc_addr) {
        if (word != target.old_val) {
          stat = kFailed;
          break;
        } else if (!addr->compare_exchange_strong(word, desc_addr, kSeqCst,
                                                  kSeqCst)) {  // CAS is failed
          continue;  // この部分の実装が怪しそう．while文の前までgotoのが良い？全然別の実装の方が良い？
        }
      }
    }
    cur_stat = stat_.load(kSeqCst);
    if (cur_stat == kUndecided && stat_.compare_exchange_strong(cur_stat, stat, kSeqCst, kSeqCst)) {
      cur_stat = stat;
    }
  }

  if (cur_stat == kSucceeded) {
    for (size_t i = 0; i < target_cnt_; ++i) {
      auto &target = targets_[i];
      auto expected = target.addr->load(kSeqCst);
      if (expected != desc_addr) continue;
      target.addr->compare_exchange_strong(expected, target.new_val, target.fence, target.fence);
    }
  } else {
    for (size_t i = 0; i < target_cnt_; ++i) {
      auto &target = targets_[i];
      auto expected = target.addr->load(kSeqCst);
      if (expected != desc_addr) continue;
      target.addr->compare_exchange_strong(expected, target.old_val, target.fence, target.fence);
    }
  }

  return cur_stat;
}

auto
MwCASDescriptor::EmbedDescriptor(  //
    const uint64_t desc_addr,
    const size_t pos)  //
    -> bool
{
  auto &target = targets_[pos];
  auto *addr = target.addr;
  auto word = addr->load(kSeqCst);
  while (true) {
    while (word & kMwCASFlag) {
      if (word == desc_addr) return true;
      const auto another_desc_addr = word;
      std::this_thread::sleep_for(kBackOffTime);
      word = addr->load(kSeqCst);
      if (word != another_desc_addr) continue;  // other threads modified this field

      // a long CPU stall has been detected, so perform another MwCAS
      auto *another_desc = std::bit_cast<MwCASDescriptor *>(another_desc_addr ^ kMwCASFlag);
      another_desc->MwCASInternal();
      word = addr->load(kSeqCst);
    }

    if (word != target.old_val) return false;
    if (addr->compare_exchange_strong(word, desc_addr, kSeqCst, kSeqCst)) return true;
  }
}

}  // namespace dbgroup::atomic::mwcas::lock_free
