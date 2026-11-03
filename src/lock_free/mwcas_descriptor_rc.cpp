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
#include "dbgroup/atomic/mwcas/lock_free/mwcas_descriptor_rc.hpp"

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
#include <dbgroup/thread/id_manager.hpp>

// local sources
#include "dbgroup/atomic/mwcas/stats.hpp"
#include "dbgroup/atomic/mwcas/utility.hpp"

//                                   Bit allocation of a word.
// |    63     |      62-60     |      59-(N+1)     |   N-41   |        40-0        |
// | MCAS Flag | Begin Position | Reference Counter | ThreadID | Descriptor Address |
//
// The reference counter is the external count of split reference counting. It
// is the number of followers that have acquired references via this word, and it
// never wraps around (followers wait while it is saturated).

//        Bit allocation of an actual value.
//          |    63     |     62-0     |
//          | MCAS Flag | Actual Value |

namespace dbgroup::atomic::mwcas::lock_free
{
namespace
{
/*############################################################################*
 * Local constants
 *############################################################################*/

/// @brief The bit width of the ThreadID field.
constexpr uint64_t kThreadIdBitNum = 13;

/// @brief An offset to shift descriptor addresses (due to 64-byte alignment).
constexpr uint64_t kAddrAlignShift = 6;

/// @brief An offset for right-shifting to extract the "thread ID".
constexpr uint64_t kThreadIdShift = 41;

/// @brief An offset for right-shifting to extract the "reference counter".
constexpr uint64_t kCntShift = kThreadIdShift + kThreadIdBitNum;

/// @brief An offset for right-shifting to extract the "begin position".
constexpr uint64_t kPosShift = 60;

/// @brief A bitmask with only the "descriptor address" portion set to 1.
constexpr uint64_t kAddrMask = (1UL << kThreadIdShift) - 1UL;

/// @brief A bitmask with only the "thread ID" portion set to 1.
constexpr uint64_t kThreadIdMask = ((1UL << kThreadIdBitNum) - 1UL) << kThreadIdShift;

/// @brief A bitmask with only the "reference counter" portion set to 1.
constexpr uint64_t kCntMask = ((1UL << (kPosShift - kCntShift)) - 1UL) << kCntShift;

/// @brief The unit of the reference counter.
constexpr uint64_t kCntUnit = 1UL << kCntShift;

/// @brief A bitmask with only the "begin position" portion set to 1.
constexpr uint64_t kPosMask = 7UL << kPosShift;

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
 * reference count) to followers.
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
  stat_.store(kUndecided, kRelease);  // set a memory fence
  const auto succeeded = MwCASInternal();

  // all the words embedded before the decision have been detached and their
  // external counts have been transferred, so the owner is the only holder if
  // the internal count is one
  if (ref_cnt_.load(kAcquire) == 1) {
    Recycle(this);
  } else {
    Release();
  }
  return succeeded;
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
    // the external count is saturated, so yield the CPU to other followers
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

  // follow another MwCAS
  auto* const another_desc =
      std::bit_cast<MwCASDescriptorRC*>((word & kAddrMask) << kAddrAlignShift);
  const auto pos = (word & kPosMask) >> kPosShift;
  if (another_desc->DetermineThreadId(pos, word)) {
    another_desc->MwCASInternal(pos + 1);
  }

  // the followed word has been detached in both the branches, so our external
  // reference has been transferred to the internal count
  another_desc->Release();
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
  MWCAS_STAT_INC(kDetachRetry);

  // cancel the transfer; the count never reaches zero here because the caller
  // holds a reference
  if (cnt > 0) {
    ref_cnt_.fetch_sub(cnt, kRelaxed);
  }
  return false;
}

void
MwCASDescriptorRC::Abort(  //
    uint64_t desc_addr,    //
    MwCASTarget& target)
{
  auto expected = target.addr->load(kAcquire);
  while ((expected & kDescMask) == desc_addr) {
    if (TryDetach(target.addr, expected, target.old_val)) return;
    CPP_UTILITY_SPINLOCK_HINT
  }
}

void
MwCASDescriptorRC::Complete(  //
    uint64_t desc_addr,       //
    MwCASTarget& target)
{
  auto expected = target.addr->load(kAcquire);
  while ((expected & kDescMask) == desc_addr) {
    // verification of thread ID
    const auto embedded_tid = (expected & kThreadIdMask) >> kThreadIdShift;
    const auto desired =
        (embedded_tid == target.thread_id.load(kRelaxed)) ? target.new_val : target.old_val;
    if (TryDetach(target.addr, expected, desired)) return;
    CPP_UTILITY_SPINLOCK_HINT
  }
}

auto
MwCASDescriptorRC::MwCASInternal(  //
    const size_t begin_pos)        //
    -> bool
{
  const auto thread_id = ::dbgroup::thread::IDManager::GetThreadID();
  const auto desc_addr = std::bit_cast<uint64_t>(this) >> kAddrAlignShift;
  const auto base_addr = kMwCASFlag | desc_addr;
  const auto owned_addr = base_addr | (thread_id << kThreadIdShift);
  auto cur_stat = stat_.load(kAcquire);  // set a memory fence for followers
  if (cur_stat == kUndecided) {
    auto stat = kSucceeded;
    for (size_t i = begin_pos; i < target_cnt_; ++i) {
      const auto desc_addr = owned_addr | (i << kPosShift);
      auto& target = targets_[i];
      auto* const addr = target.addr;
      const auto expected = target.old_val;
      const auto fence = WithAcqRel(target.fence);
      auto word = addr->load(kAcquire);

      // try to embed the descriptor
      if (word == expected && addr->compare_exchange_strong(word, desc_addr, fence, kAcquire)) {
        if (DetermineThreadId(i, desc_addr)) continue;
        break;  // this MCAS has been completed
      }

      // check another thread has embedded the descriptor
      if ((word & kDescMask) != base_addr) {
        stat = kFailed;
        break;
      }
      if (!DetermineThreadId(i, word)) break;  // this MCAS has been completed
    }

    // set a linearization point
    cur_stat = stat_.load(kRelaxed);
    if (cur_stat == kUndecided
        && stat_.compare_exchange_strong(cur_stat, stat, kRelaxed, kRelaxed)) {
      cur_stat = stat;
    }
  }

  // detach all the embedded descriptors (including duplicated ones)
  const auto succeeded = (cur_stat == kSucceeded);
  if (succeeded) {
    for (size_t i = 0; i < target_cnt_; ++i) {
      Complete(base_addr, targets_[i]);
    }
  } else {
    for (size_t i = 0; i < target_cnt_; ++i) {
      Abort(base_addr, targets_[i]);
    }
  }

  return succeeded;
}

auto
MwCASDescriptorRC::DetermineThreadId(  //
    const size_t pos,                  //
    const uint64_t word)               //
    -> bool
{
  auto& target = targets_[pos];
  const auto embedded_tid = (word & kThreadIdMask) >> kThreadIdShift;
  auto current_tid = target.thread_id.load(kRelaxed);

  if (current_tid == kInitialThreadId
      && target.thread_id.compare_exchange_strong(current_tid, embedded_tid, kRelaxed, kRelaxed)) {
    return true;
  }
  if (current_tid == embedded_tid) return true;

  // clean up the duplicated embedded descriptor (retry if followers have
  // incremented the counter)
  auto cur = word;
  while (((cur ^ word) & ~kCntMask) == 0) {
    if (TryDetach(target.addr, cur, target.old_val)) break;
    CPP_UTILITY_SPINLOCK_HINT
  }
  return false;
}

}  // namespace dbgroup::atomic::mwcas::lock_free
