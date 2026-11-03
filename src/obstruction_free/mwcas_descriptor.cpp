/*
 * Copyright 2025 Database Group, Nagoya University
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
#include "dbgroup/atomic/mwcas/obstruction_free/mwcas_descriptor.hpp"

// C++ standard libraries
#include <algorithm>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>

// external C++ libraries
#include <dbgroup/lock/utility.hpp>
#include <dbgroup/memory/epoch_based_gc.hpp>

// local sources
#include "dbgroup/atomic/mwcas/stats.hpp"
#include "dbgroup/atomic/mwcas/utility.hpp"

//                        Bit allocation of a word.
// |    63     |  62-60   |      59-41        |        40-0        |
// | MCAS Flag | Position | Reference Counter | Descriptor Address |
//
// The thread ID field is not needed because only the owner embeds its
// descriptor. The position is not used by the algorithm (only for debugging).
// The reference counter is used for back-off and for detecting followers, and
// it never wraps around.

//        Bit allocation of an actual value.
//          |    63     |     62-0     |
//          | MCAS Flag | Actual Value |

namespace dbgroup::atomic::mwcas::obstruction_free
{
namespace
{
/*############################################################################*
 * Local constants
 *############################################################################*/

/// @brief An offset to shift descriptor addresses (due to 64-byte alignment).
constexpr uint64_t kAddrAlignShift = 6;

/// @brief An offset for right-shifting to extract the "reference counter".
constexpr uint64_t kCntShift = 41;

/// @brief An offset for right-shifting to extract the "position".
constexpr uint64_t kPosShift = 60;

/// @brief A bitmask with only the "descriptor address" portion set to 1.
constexpr uint64_t kAddrMask = (1UL << kCntShift) - 1UL;

/// @brief A bitmask with only the "reference counter" portion set to 1.
constexpr uint64_t kCntMask = ((1UL << (kPosShift - kCntShift)) - 1UL) << kCntShift;

/// @brief The unit of the reference counter.
constexpr uint64_t kCntUnit = 1UL << kCntShift;

/// @brief A bitmask with only the "MwCAS FLAG" and "descriptor address" portions set to 1.
constexpr uint64_t kDescMask = kMwCASFlag | kAddrMask;

/// @brief The maximum exponent of back-off (to prevent overflow).
constexpr uint64_t kMaxBackOffExp = 20;

/*############################################################################*
 * Local utilities
 *############################################################################*/

/**
 * @param fence A memory fence given by users.
 * @return A memory fence that includes at least release semantics.
 * @note Embedding a descriptor must publish its targets to followers.
 */
constexpr auto
WithRelease(  //
    const std::memory_order fence)  //
    -> std::memory_order
{
  return (fence == std::memory_order_seq_cst) ? fence : std::memory_order_acq_rel;
}

/*############################################################################*
 * Local global variable
 *############################################################################*/

/// @brief A thread local descriptor for reuse.
thread_local std::unique_ptr<MwCASDescriptor> _tls = nullptr;  // NOLINT

}  // namespace

/*############################################################################*
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

/*############################################################################*
 * Public APIs
 *############################################################################*/

auto
MwCASDescriptor::GetDescriptor()  //
    -> MwCASDescriptor*
{
  auto* desc = _tls.release();
  if (!desc) {
    auto* const page = _gc->GetPageIfPossible<MwCASDescriptor>();
    if (page) {
      desc = static_cast<MwCASDescriptor*>(page);
    } else {
      desc = new MwCASDescriptor{};
      MWCAS_STAT_INC(kDescNew);
    }
  }
  desc->target_cnt_ = 0;
  return desc;
}

auto
MwCASDescriptor::MwCAS()  //
    -> bool
{
  stat_.store(kUndecided, kRelease);  // publish targets to followers
  const auto base_addr = kMwCASFlag | (std::bit_cast<uint64_t>(this) >> kAddrAlignShift);

  // embed this descriptor into all the targets (only the owner embeds)
  auto stat = kSucceeded;
  size_t embedded_cnt = 0;
  for (; embedded_cnt < target_cnt_; ++embedded_cnt) {
    auto& target = targets_[embedded_cnt];
    auto word = target.addr->load(kRelaxed);
    if (word != target.old_val
        || !target.addr->compare_exchange_strong(word, base_addr | (embedded_cnt << kPosShift),
                                                 WithRelease(target.fence), kRelaxed)) {
      stat = kFailed;  // a different value or another descriptor
      break;
    }
  }

  // set a linearization point; a reader may have already aborted this MwCAS
  auto cur_stat = kUndecided;
  if (!stat_.compare_exchange_strong(cur_stat, stat, kRelaxed, kRelaxed)) {
    stat = cur_stat;  // always kFailed
  }

  // detach this descriptor and retire it
  const auto referred = Finish(base_addr, stat, embedded_cnt);
  if (referred) {
    _gc->AddGarbage<MwCASDescriptor>(this);
  } else {
    _tls.reset(this);
  }

  return stat == kSucceeded;
}

/*############################################################################*
 * Internal APIs
 *############################################################################*/

auto
MwCASDescriptor::PrepareToFollow(  //
    std::atomic_uint64_t* const addr,
    uint64_t& word,
    const std::memory_order fence)  //
    -> bool
{
  if constexpr (kUseBackOff) {
    // wait for other threads to make progress on the incomplete MwCAS
    const auto expected = word;
    for (uint32_t i = 0; i < kRetryNum; ++i) {
      CPP_UTILITY_SPINLOCK_HINT
      word = addr->load(fence);
      if (word != expected) return false;
    }

    const auto count = std::min((word & kCntMask) >> kCntShift, kMaxBackOffExp);
    std::this_thread::sleep_for(kBackOffTime * (1UL << count));  // exponential back-off
    word = addr->load(fence);
    if (word != expected) return false;  // other threads modified this field
  } else {
    // the counter is used only as a "referred" flag
    if ((word & kCntMask) != 0) return true;
  }

  // register this thread as a follower (the counter never wraps around)
  if ((word & kCntMask) == kCntMask) [[unlikely]] return true;
  return addr->compare_exchange_strong(word, word + kCntUnit, kRelaxed, fence);
}

void
MwCASDescriptor::FollowIfNeeded(  //
    std::atomic_uint64_t* const addr,
    uint64_t& word,
    const std::memory_order fence)
{
  if (!PrepareToFollow(addr, word, fence)) return;
  MWCAS_STAT_INC(kFollow);

  auto* const desc = std::bit_cast<MwCASDescriptor*>((word & kAddrMask) << kAddrAlignShift);
  const auto base_addr = word & kDescMask;

  // abort the MwCAS if it has not been decided (synchronize with the owner to
  // read its targets)
  auto stat = kUndecided;
  if (desc->stat_.compare_exchange_strong(stat, kFailed, std::memory_order_acq_rel, kAcquire)) {
    stat = kFailed;
    MWCAS_STAT_INC(kAbortByReader);
  }

  // detach the descriptor (readers never embed it)
  desc->Finish(base_addr, stat, 0);
  word = addr->load(fence);
}

auto
MwCASDescriptor::Finish(  //
    const uint64_t base_addr,
    const Status stat,
    const size_t embedded_cnt)  //
    -> bool
{
  // every embedded descriptor is the owner's one, so it can be replaced with a
  // new value without any verification if this MwCAS has succeeded
  auto referred = false;
  for (size_t i = 0; i < target_cnt_; ++i) {
    auto& target = targets_[i];
    const auto desired = (stat == kSucceeded) ? target.new_val : target.old_val;
    auto expected = target.addr->load(kRelaxed);
    auto detached = false;
    while ((expected & kDescMask) == base_addr) {
      if (target.addr->compare_exchange_weak(expected, desired, kRelaxed, kRelaxed)) {
        if ((expected & kCntMask) != 0) {
          referred = true;
          MWCAS_STAT_INC(kDetachWithCnt);
        }
        detached = true;
        break;
      }
      MWCAS_STAT_INC(kDetachRetry);
      CPP_UTILITY_SPINLOCK_HINT
    }

    // another thread has detached the owner's embedded descriptor
    if (!detached && i < embedded_cnt) {
      referred = true;
    }
  }
  return referred;
}

}  // namespace dbgroup::atomic::mwcas::obstruction_free
