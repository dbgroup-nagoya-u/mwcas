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
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>

// external libraries
#include "dbgroup/lock/common.hpp"
#include "dbgroup/memory/epoch_based_gc.hpp"
#include "dbgroup/memory/utility.hpp"

// local sources
#include "dbgroup/atomic/mwcas/utility.hpp"

//                       Bit allocation of a word.
// |     63     |       62-50       |      49-47     |         46-0       |
// | MwCAS Flag | Reference Counter | Begin Position | Descriptor Address |

//                   Bit allocation of an actual value.
//          |           63           |  62-(N+1) |     N-0      |
//          | Version Confirmed Flag |  Version  | Actual Value |

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

/// @brief A bitmask with only the "descriptor address" portion set to 1.
constexpr uint64_t kAddrMask = (1UL << 47UL) - 1UL;

/// @brief A bitmask with only the "begin position" portion set to 1.
constexpr uint64_t kPosMask = (kCntUnit - 1UL) ^ kAddrMask;

/// @brief A bitmask with only the "reference counter" portion set to 1.
constexpr uint64_t kCntMask = (kMwCASFlag - 1UL) ^ (kPosMask | kAddrMask);

/// @brief A bitmask with only the "MwCAS FLAG" and "descriptor address" portions set to 1.
constexpr uint64_t kDescMask = kMwCASFlag | kAddrMask;

}  // namespace

/*##############################################################################
 * Static utilities
 *############################################################################*/

void
MwCASDescriptor::StartGC(  //
    const size_t gc_interval,
    const size_t gc_thread_num)
{
  _gc = std::make_unique<EpochBasedGC>(gc_interval, gc_thread_num, kMaxReusableDescriptors);
}

void
MwCASDescriptor::StopGC()
{
  _gc.reset();
}

auto
MwCASDescriptor::CreateEpochGuard()  //
    -> ::dbgroup::thread::EpochGuard
{
  return _gc->CreateEpochGuard();
}

/*##############################################################################
 * Public APIs
 *############################################################################*/

auto
MwCASDescriptor::GetDescriptor()  //
    -> MwCASDescriptor *
{
  auto *page = _gc->GetPageIfPossible<MwCASDescriptor>();
  auto *desc = (page) ? static_cast<MwCASDescriptor *>(page) : new MwCASDescriptor{};
  desc->target_cnt_ = 0;
  return desc;
}

auto
MwCASDescriptor::MwCAS()  //
    -> bool
{
  stat_.store(kUndecided, kRelease);  // set a memory fence
  const auto succeeded = MwCASInternal();
  _gc->AddGarbage<MwCASDescriptor>(this);
  return succeeded;
}

/*##############################################################################
 * Internal APIs
 *############################################################################*/

void
MwCASDescriptor::FollowIfNeeded(  //
    std::atomic_uint64_t *addr,
    uint64_t &word,
    const std::memory_order fence)
{
  const auto another_word = word;
  std::this_thread::sleep_for(kBackOffTime);
  word = addr->load(fence);
  if (word != another_word) return;  // other threads modified this field

  // a long CPU stall has been detected, so perform another MwCAS
  const auto incremented = word + kCntUnit;
  if (addr->compare_exchange_strong(word, incremented, kRelaxed, fence)) {
    // follow another MwCAS
    auto *another_desc = std::bit_cast<MwCASDescriptor *>(word & kAddrMask);
    const auto pos = (word & kPosMask) >> kPosShift;
    another_desc->MwCASInternal(pos + 1);
    word = addr->load(fence);
  }
}

auto
MwCASDescriptor::MwCASInternal(  //
    const size_t begin_pos)      //
    -> bool
{
  const auto base_addr = std::bit_cast<uint64_t>(this) | kMwCASFlag;
  auto cur_stat = stat_.load(kAcquire);  // set a memory fence for followers
  if (cur_stat == kUndecided) {
    auto stat = kSucceeded;
    for (size_t i = begin_pos; i < target_cnt_; ++i) {
      const auto desc_addr = base_addr | (i << kPosShift);
      auto &target = targets_[i];
      auto *addr = target.addr;
      auto &old_val = target.old_val;
      const auto fence = target.fence;
      auto word = addr->load(kRelaxed);

      // retain the current version and try to embed the descriptor
      auto expected = old_val.load(kRelaxed);
      if (((expected & kMwCASFlag) == 0 && ((word ^ expected) & ~kVersionMask) == 0
           && old_val.compare_exchange_strong(expected, word | kMwCASFlag, kRelaxed, kRelaxed)
           && addr->compare_exchange_strong(word, desc_addr, fence, kRelaxed))) {
        continue;
      }

      // this thread may be a follower, so use the retained word
      if (word == expected && addr->compare_exchange_strong(word, desc_addr, fence, kRelaxed)) {
        continue;
      }

      // check another thread has embedded the descriptor
      if ((word & kDescMask) != base_addr) {
        stat = kFailed;
        break;
      }
    }

    // set a linearization point
    cur_stat = stat_.load(kRelaxed);
    if (cur_stat == kUndecided
        && stat_.compare_exchange_strong(cur_stat, stat, kRelaxed, kRelaxed)) {
      cur_stat = stat;
    }
  }

  const auto succeeded = (cur_stat == kSucceeded);
  if (succeeded) {
    for (size_t i = 0; i < target_cnt_; ++i) {
      auto &target = targets_[i];
      const auto ver = (target.old_val.load(kRelaxed) + kVersionUnit) & kVersionMask;
      Finalize(base_addr, target, (target.new_val | ver));
    }
  } else {
    for (size_t i = 0; i < target_cnt_; ++i) {
      auto &target = targets_[i];
      const auto val = (target.old_val.load(kRelaxed) + kVersionUnit) & kVerAndValMask;
      Finalize(base_addr, target, val);
    }
  }

  return succeeded;
}

bool
MwCASDescriptor::Finalize(  //
    const uint64_t desc_addr,
    MwCASTarget &target,
    const uint64_t desired)  //
{
  auto expected = target.addr->load(kRelaxed);
  while (((expected ^ desc_addr) & kDescMask) == 0
         && !target.addr->compare_exchange_weak(expected, desired, kRelaxed, kRelaxed)) {
    CPP_UTILITY_SPINLOCK_HINT
  }
  return (expected & kCntMask) == 0;
}

}  // namespace dbgroup::atomic::mwcas::lock_free
