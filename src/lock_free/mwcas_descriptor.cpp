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

//                wordのビット割り当て（実装後に消す）
// |     63     |       62-50       |      49-47     |        46-0       |
// | MwCAS Flag | Reference Counter | Begin Position |Descriptor Address |

auto
MwCASDescriptor::MwCASInternal(  //
    const size_t begin_pos       //
    ) -> bool
{
  const auto desc_addr = std::bit_cast<uint64_t>(this) | kMwCASFlag;
  auto cur_stat = stat_.load(kSeqCst);
  if (cur_stat == kUndecided) {
    auto stat = kSucceeded;
    for (size_t i = begin_pos; i < target_cnt_; ++i) {
      auto &target = targets_[i];
      if (!EmbedDescriptor(desc_addr, i, target.cnt)) {
        stat = kFailed;
        break;
      }
    }
    cur_stat = stat_.load(kSeqCst);
    if (cur_stat == kUndecided && stat_.compare_exchange_strong(cur_stat, stat, kSeqCst, kSeqCst)) {
      cur_stat = stat;
    }
  }

  auto ret = (cur_stat == kSucceeded);
  auto target_cnt_sum = 0;
  if (ret) {
    for (size_t i = 0; i < target_cnt_; ++i) {
      auto &target = targets_[i];
      auto expected = target.addr->load(kSeqCst);
      if ((expected ^ desc_addr) & kAddrMask) continue;
      target.addr->compare_exchange_strong(expected, target.new_val, target.fence, target.fence);
      target_cnt_sum += target.cnt;
    }
  } else {
    for (size_t i = 0; i < target_cnt_; ++i) {
      auto &target = targets_[i];
      auto expected = target.addr->load(kSeqCst);
      if ((expected ^ desc_addr) & kAddrMask) continue;
      target.addr->compare_exchange_strong(expected, target.old_val, target.fence, target.fence);
      target_cnt_sum += target.cnt;
    }
  }

  exit_cnt_++;
  if (exit_cnt_ == target_cnt_sum) {
    // GC処理（未実装）
  }

  return ret;
}

auto
MwCASDescriptor::EmbedDescriptor(  //
    const uint64_t desc_addr,
    const size_t pos,
    const size_t cnt)  //
    -> bool
{
  auto &target = targets_[pos];
  auto *addr = target.addr;
  auto word = addr->load(kSeqCst);
  while (true) {
    while (word & kMwCASFlag) {
      if (((word ^ desc_addr) & kAddrMask) == 0) return true;
      const auto another_word = word;
      std::this_thread::sleep_for(kBackOffTime);
      word = addr->load(kSeqCst);
      if (((word ^ another_word) | ~kCntMask) == 0)  // Equal except for cnt.
        continue;                                    // other threads modified this field

      // a long CPU stall has been detected, so perform another MwCAS
      auto *another_word_pointer = std::bit_cast<MwCASDescriptor *>(another_word & kAddrMask);

      // ここで支援する記述子のcntをインクリメントしたものをCAS(もしくはTATAS)して記述子を埋め込みなおす．その後に支援
      auto another_word_cnt = ((another_word & kCntMask) >> kCntShift);
      auto another_word_pos = ((another_word & kPosMask) >> kPositionShift);
      auto another_word_incremented_cnt =
          ((another_word & ~kCntMask) | ((another_word_cnt + 1) << kCntShift));
      if (addr->compare_exchange_strong(
              another_word, another_word_incremented_cnt, kSeqCst,
              kSeqCst)) {  // ここでビルドエラーが出る（another_wordの型エラーだとは思う）
        (another_word_pointer->targets_[another_word_pos])
            .cnt++;  // これでインクリメントできるのかと設計上安全なのかはわからない．
        another_word_pointer->MwCASInternal(another_word_pos + 1);
      } else {
        // 何もしなければ勝手にバックオフになるはず
      }
      word = addr->load(kSeqCst);
    }

    if (word != target.old_val) return false;
    if (addr->compare_exchange_strong(word, desc_addr, kSeqCst, kSeqCst)) return true;
  }
}

}  // namespace dbgroup::atomic::mwcas::lock_free
