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
#include "dbgroup/atomic/mwcas/obstruction_free/mwcas_descriptor_rc.hpp"

// C++ standard libraries
#include <algorithm>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

// external C++ libraries
#include <dbgroup/lock/utility.hpp>

// local sources
#include "dbgroup/atomic/mwcas/stats.hpp"
#include "dbgroup/atomic/mwcas/utility.hpp"

//                        Bit allocation of a word.
// |    63     |  62-60   |      59-41        |        40-0        |
// | MCAS Flag | Position | Reference Counter | Descriptor Address |
//
// The thread ID field is not needed because only the owner embeds its
// descriptor. The position is not used by the algorithm (only for debugging).
// The reference counter is the external count of split reference counting. It
// is the number of readers that have acquired references via this word, and it
// never wraps around (readers wait while it is saturated).

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
 * @return A memory fence that includes at least acquire and release semantics.
 * @note Embedding a descriptor must publish its fields (including the internal
 * reference count) to readers.
 */
constexpr auto
WithAcqRel(  //
    const std::memory_order fence)  //
    -> std::memory_order
{
  return (fence == std::memory_order_seq_cst) ? fence : std::memory_order_acq_rel;
}

/*############################################################################*
 * Local global variable
 *############################################################################*/

/// @brief A thread local descriptor for reuse.
thread_local std::unique_ptr<MwCASDescriptorRC> _tls = nullptr;  // NOLINT

/// @brief Thread local descriptors that have been released by this thread.
thread_local std::vector<std::unique_ptr<MwCASDescriptorRC>> _pool{};  // NOLINT

}  // namespace

/*############################################################################*
 * Public APIs
 *############################################################################*/

auto
MwCASDescriptorRC::GetDescriptor()  //
    -> MwCASDescriptorRC*
{
  auto* desc = _tls.release();
  if (!desc) {
    if (_pool.empty()) {
      desc = new MwCASDescriptorRC{};
      MWCAS_STAT_INC(kDescNew);
    } else {
      desc = _pool.back().release();
      _pool.pop_back();
    }
  }
  desc->ref_cnt_.store(1, kRelaxed);  // the owner's reference
  desc->target_cnt_ = 0;
  return desc;
}

auto
MwCASDescriptorRC::MwCAS()  //
    -> bool
{
  stat_.store(kUndecided, kRelease);  // publish targets to readers
  const auto base_addr = kMwCASFlag | (std::bit_cast<uint64_t>(this) >> kAddrAlignShift);

  // embed this descriptor into all the targets (only the owner embeds)
  auto stat = kSucceeded;
  for (size_t i = 0; i < target_cnt_; ++i) {
    auto& target = targets_[i];
    auto word = target.addr->load(kRelaxed);
    if (word != target.old_val
        || !target.addr->compare_exchange_strong(word, base_addr | (i << kPosShift),
                                                 WithAcqRel(target.fence), kRelaxed)) {
      stat = kFailed;  // a different value or another descriptor
      break;
    }
  }

  // set a linearization point; a reader may have already aborted this MwCAS
  auto cur_stat = kUndecided;
  if (!stat_.compare_exchange_strong(cur_stat, stat, kRelaxed, kRelaxed)) {
    stat = cur_stat;  // always kFailed
  }

  // detach this descriptor; after that, no word embeds it and all the external
  // counts have been transferred, so the owner is the only holder if the
  // internal count is one
  Finish(base_addr, stat);
  if (ref_cnt_.load(kAcquire) == 1) {
    Recycle(this);
  } else {
    Release();
  }

  return stat == kSucceeded;
}

/*############################################################################*
 * Internal APIs
 *############################################################################*/

auto
MwCASDescriptorRC::PrepareToFollow(  //
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
  }

  if ((word & kCntMask) == kCntMask) [[unlikely]] {
    // the external count is saturated, so yield the CPU to other readers
    MWCAS_STAT_INC(kSaturationWait);
    std::this_thread::yield();
    word = addr->load(fence);
    return false;
  }

  // acquire a reference
  const auto incremented = word + kCntUnit;
  if (!addr->compare_exchange_strong(word, incremented, kAcquire, fence)) return false;
  word = incremented;
  return true;
}

void
MwCASDescriptorRC::FollowIfNeeded(  //
    std::atomic_uint64_t* const addr,
    uint64_t& word,
    const std::memory_order fence)
{
  if (!PrepareToFollow(addr, word, fence)) return;
  MWCAS_STAT_INC(kFollow);

  auto* const desc =
      std::bit_cast<MwCASDescriptorRC*>((word & kAddrMask) << kAddrAlignShift);
  const auto base_addr = word & kDescMask;

  // abort the MwCAS if it has not been decided
  auto stat = kUndecided;
  if (desc->stat_.compare_exchange_strong(stat, kFailed, std::memory_order_acq_rel, kAcquire)) {
    stat = kFailed;
    MWCAS_STAT_INC(kAbortByReader);
  }

  // detach the descriptor (readers never embed it); the followed word has been
  // detached after that, so our external reference has been transferred
  desc->Finish(base_addr, stat);
  desc->Release();
  word = addr->load(fence);
}

void
MwCASDescriptorRC::Recycle(  //
    MwCASDescriptorRC* const desc)
{
  if (!_tls) {
    _tls.reset(desc);
  } else if (_pool.size() < kMaxReusableDescriptors) {
    _pool.emplace_back(desc);
  } else {
    delete desc;
  }
}

void
MwCASDescriptorRC::Release()
{
  if (ref_cnt_.fetch_sub(1, kRelease) == 1) {
    std::atomic_thread_fence(kAcquire);
    Recycle(this);
  }
}

auto
MwCASDescriptorRC::TryDetach(  //
    std::atomic_uint64_t* const addr,
    uint64_t& expected,
    const uint64_t desired)  //
    -> bool
{
  // transfer the external count before the detachment becomes visible
  const auto cnt = static_cast<int64_t>((expected & kCntMask) >> kCntShift);
  if (cnt > 0) {
    ref_cnt_.fetch_add(cnt, kRelaxed);
  }
  if (addr->compare_exchange_strong(expected, desired, kRelease, kAcquire)) {
    if (cnt > 0) {
      MWCAS_STAT_INC(kDetachWithCnt);
    }
    return true;
  }

  // cancel the transfer; the count never reaches zero here because the caller
  // holds a reference
  MWCAS_STAT_INC(kDetachRetry);
  if (cnt > 0) {
    ref_cnt_.fetch_sub(cnt, kRelaxed);
  }
  return false;
}

void
MwCASDescriptorRC::Finish(  //
    const uint64_t base_addr,
    const Status stat)
{
  // every embedded descriptor is the owner's one, so it can be replaced with a
  // new value without any verification if this MwCAS has succeeded
  for (size_t i = 0; i < target_cnt_; ++i) {
    auto& target = targets_[i];
    const auto desired = (stat == kSucceeded) ? target.new_val : target.old_val;
    auto expected = target.addr->load(kAcquire);
    while ((expected & kDescMask) == base_addr) {
      if (TryDetach(target.addr, expected, desired)) break;
      CPP_UTILITY_SPINLOCK_HINT
    }
  }
}

}  // namespace dbgroup::atomic::mwcas::obstruction_free
