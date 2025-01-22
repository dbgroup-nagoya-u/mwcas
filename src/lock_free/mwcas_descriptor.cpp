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
#include "dbgroup/memory/utility.hpp"

// local sources
#include "dbgroup/atomic/mwcas/utility.hpp"

//                       Bit allocation of a word.
// |     63     |       62-50       |      49-47     |        46-0       |
// | MwCAS Flag | Reference Counter | Begin Position |Descriptor Address |

namespace dbgroup::atomic::mwcas::lock_free
{
namespace
{
/*##############################################################################
 * Local constants
 *############################################################################*/

/// @brief An offset for right-shifting to extract the "begin position".
constexpr uint64_t kPosShift = 47;

/// @brief An offset for right-shifting to extract the "reference counter".
constexpr uint64_t kCntShift = 50;

/// @brief A constant for incrementing the "reference counter".
constexpr uint64_t kCntUnit = 1UL << kCntShift;

/// @brief A bitmask with only the "begin position" portion set to 1.
constexpr uint64_t kPosMask = (((1UL << (1 + kCntShift - kPosShift)) - 1) << kPosShift);

/// @brief A bitmask with only the "reference counter" portion set to 1.
constexpr uint64_t kCntMask = (((1UL << (64 - kCntShift)) - 1) << kCntShift);

/// @brief A bitmask with only the "descriptor address" portion set to 1.
constexpr uint64_t kAddrMask = (1UL << 47) - 1;

/// @brief A bitmask with only the "MwCAS FLAG" and "descriptor address" portions set to 1.
constexpr uint64_t kDescMask = kMwCASFlag | kAddrMask;

/*##############################################################################
 * Local global variable
 *############################################################################*/
thread_local std::unique_ptr<MwCASDescriptor> tls = nullptr;

}  // namespace

/*##############################################################################
 * Static utilities
 *############################################################################*/

auto
MwCASDescriptor::GetDescriptor()  //
    -> MwCASDescriptor *
{
  auto *desc = tls.release();
  if (!desc) {
    desc = ::dbgroup::memory::Allocate<MwCASDescriptor>();
  }
  desc->target_cnt_ = 0;
  desc->exit_cnt_.store(0, kRelaxed);
  return desc;
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
    const size_t begin_pos       //
    ) -> bool
{
  const auto desc_addr = std::bit_cast<uint64_t>(this) | kMwCASFlag;
  auto cur_stat = stat_.load(kSeqCst);
  if (cur_stat == kUndecided) {
    auto stat = kSucceeded;
    for (size_t i = begin_pos; i < target_cnt_; ++i) {
      if (!EmbedDescriptor(desc_addr, i)) {
        stat = kFailed;
        break;
      }
    }
    cur_stat = stat_.load(kSeqCst);
    if (cur_stat == kUndecided && stat_.compare_exchange_strong(cur_stat, stat, kSeqCst, kSeqCst)) {
      cur_stat = stat;
    }
  }

  const auto succeeded = (cur_stat == kSucceeded);
  auto follow_cnt = 0;
  if (succeeded) {
    for (size_t i = 0; i < target_cnt_; ++i) {
      auto &target = targets_[i];
      auto expected = target.addr->load(kSeqCst);
      if (((expected ^ desc_addr) & kDescMask) == 0UL) {
        target.addr->compare_exchange_strong(expected, target.new_val, target.fence, target.fence);
        follow_cnt += ((expected & kCntMask) >> kCntShift);
      } else {
        follow_cnt += target.cnt;
      }
    }
  } else {
    for (size_t i = 0; i < target_cnt_; ++i) {
      auto &target = targets_[i];
      auto expected = target.addr->load(kSeqCst);
      if (((expected ^ desc_addr) & kDescMask) == 0UL) {
        target.addr->compare_exchange_strong(expected, target.new_val, target.fence, target.fence);
        follow_cnt += ((expected & kCntMask) >> kCntShift);
      } else {
        follow_cnt += target.cnt;
      }
    }
  }

  if (follow_cnt == 0 || exit_cnt_.fetch_add(1, kSeqCst) == follow_cnt) {
    tls.reset(this);
  }

  return succeeded;
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
      if (((word ^ desc_addr) & kDescMask) == 0) return true;
      const auto another_word = word;
      std::this_thread::sleep_for(kBackOffTime);
      word = addr->load(kSeqCst);
      if (word != another_word) continue;  // other threads modified this field

      // a long CPU stall has been detected, so perform another MwCAS
      const auto incremented = word + kCntUnit;
      if (addr->compare_exchange_strong(word, incremented, kSeqCst, kSeqCst)) {
        // follow another MwCAS
        auto *another_desc = std::bit_cast<MwCASDescriptor *>(word & kAddrMask);
        const auto pos = (word & kPosMask) >> kPosShift;
        auto &desc_cnt = another_desc->targets_[pos].cnt;
        auto cur_cnt = desc_cnt.load(kSeqCst);
        const auto cnt = static_cast<uint32_t>((incremented & kCntMask) >> kCntShift);
        while (cur_cnt < cnt) {  // increment the descriptor's counter
          if (desc_cnt.compare_exchange_weak(cur_cnt, cnt, kSeqCst, kSeqCst)) break;
          CPP_UTILITY_SPINLOCK_HINT
        }
        another_desc->MwCASInternal(pos + 1);
        word = addr->load(kSeqCst);
      }
    }

    if (word != target.old_val) return false;
    if (addr->compare_exchange_strong(word, desc_addr, kSeqCst, kSeqCst)) return true;
  }
}

}  // namespace dbgroup::atomic::mwcas::lock_free
