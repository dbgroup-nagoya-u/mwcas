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
#include "dbgroup/atomic/mwcas/lock_free/mwcas_descriptor.hpp"

// C++ standard libraries
#include <atomic>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <utility>

// external C++ libraries
#include <dbgroup/lock/utility.hpp>
#include <dbgroup/memory/epoch_based_gc.hpp>
#include <dbgroup/thread/id_manager.hpp>

// local sources
#include "dbgroup/atomic/mwcas/stats.hpp"
#include "dbgroup/atomic/mwcas/utility.hpp"

//                                   Bit allocation of a word.
// |    63     |      62-60     |      59-(N+1)     |   N-41   |        40-0        |
// | MCAS Flag | Begin Position | Reference Counter | ThreadID | Descriptor Address |
//

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

/// @brief A bitmask with only the "begin position" portion set to 1.
constexpr uint64_t kPosMask = 7UL << kPosShift;

/// @brief A bitmask with only the "MwCAS FLAG" and "descriptor address" portions set to 1.
constexpr uint64_t kDescMask = kMwCASFlag | kAddrMask;

/*############################################################################*
 * Local utilities
 *############################################################################*/

/**
 * @param fence A memory fence given by users.
 * @return A memory fence that includes at least acquire and release semantics.
 * @note Embedding a descriptor must publish its targets to followers (H3).
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
    desc = static_cast<MwCASDescriptor*>(_gc->GetPageIfPossible<MwCASDescriptor>());
  }
  if (!desc) {
    desc = new MwCASDescriptor{};
    MWCAS_STAT_INC(kDescNew);
  }
  desc->target_cnt_ = 0;
  return desc;
}

auto
MwCASDescriptor::MwCAS()  //
    -> bool
{
  thread_local CompletedDescriptors completed_descriptors{};

  stat_.store(kUndecided, kRelease);  // set a memory fence
  auto referred = false;
  const auto succeeded = MwCASInternal(0, &referred);

  if (referred) {
    // stale helpers may still embed this descriptor, so it cannot be reused or
    // passed to GC immediately
    completed_descriptors.Retire(this);
  } else {
    // no other thread has referred to this descriptor
    _tls.reset(this);
  }
  return succeeded;
}

/*############################################################################*
 * Internal APIs
 *############################################################################*/

void
MwCASDescriptor::FollowIfNeeded(  //
    std::atomic_uint64_t* const addr,
    uint64_t& word,
    const std::memory_order fence)
{
  if (!PrepareToFollow<kCntMask, kCntShift, true>(addr, word, fence)) return;
  std::atomic_thread_fence(kAcquire);  // synchronize with the embedding (H3)
  MWCAS_STAT_INC(kFollow);

  // follow another MwCAS
  auto* const another_desc =
      std::bit_cast<MwCASDescriptor*>((word & kAddrMask) << kAddrAlignShift);
  const auto pos = (word & kPosMask) >> kPosShift;
  if (another_desc->DetermineThreadId(pos, word)) {
    another_desc->MwCASInternal(pos + 1);
  }
  word = addr->load(fence);
}

auto
MwCASDescriptor::Abort(  //
    uint64_t desc_addr,  //
    MwCASTarget& target) -> uint64_t
{
  auto expected = target.addr->load(kRelaxed);
  while (((expected ^ desc_addr) & kDescMask) == 0) {
    if (target.addr->compare_exchange_weak(expected, target.old_val, kRelaxed, kRelaxed)) {
      if ((expected & kCntMask) != 0) {
        MWCAS_STAT_INC(kDetachWithCnt);
      }
      return expected;
    }
    MWCAS_STAT_INC(kDetachRetry);
    CPP_UTILITY_SPINLOCK_HINT
  }
  return 0;
}

auto
MwCASDescriptor::Complete(  //
    uint64_t desc_addr,     //
    MwCASTarget& target) -> uint64_t
{
  auto expected = target.addr->load(kRelaxed);
  while (((expected ^ desc_addr) & kDescMask) == 0) {
    // verification of thread ID: a duplicated embedded descriptor is restored
    const auto embedded_tid = (expected & kThreadIdMask) >> kThreadIdShift;
    const auto desired =
        (embedded_tid == target.thread_id.load(kRelaxed)) ? target.new_val : target.old_val;
    if (target.addr->compare_exchange_weak(expected, desired, kRelaxed, kRelaxed)) {
      if ((expected & kCntMask) != 0) {
        MWCAS_STAT_INC(kDetachWithCnt);
      }
      return expected;
    }
    MWCAS_STAT_INC(kDetachRetry);
    CPP_UTILITY_SPINLOCK_HINT
  }
  return 0;
}

auto
MwCASDescriptor::MwCASInternal(  //
    const size_t begin_pos,      //
    bool* const referred)        //
    -> bool
{
  // only the owner tracks its embeddings to detect followers (H10): a first
  // follower always increments the counter of an embedding by the owner
  // because helpers embed this descriptor only after following
  uint64_t owned_mask = 0;
  auto is_referred = false;

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
      auto word = addr->load(kRelaxed);

      // try to embed the descriptor
      if (word == expected && addr->compare_exchange_strong(word, desc_addr, fence, kRelaxed)) {
        if (DetermineThreadId(i, desc_addr)) {
          owned_mask |= 1UL << i;
          continue;
        }
        is_referred = true;
        break;  // this MCAS has been completed
      }

      // check another thread has embedded the descriptor
      if ((word & kDescMask) != base_addr) {
        stat = kFailed;
        break;
      }
      // a helper has embedded the descriptor
      is_referred = true;
      if (!DetermineThreadId(i, word)) break;  // this MCAS has been completed
    }

    // set a linearization point
    cur_stat = stat_.load(kRelaxed);
    if (cur_stat == kUndecided
        && stat_.compare_exchange_strong(cur_stat, stat, kRelaxed, kRelaxed)) {
      cur_stat = stat;
    } else {
      is_referred = true;  // a helper has decided the status
    }
  } else {
    is_referred = true;
  }

  // detach all the embedded descriptors (including duplicated ones)
  const auto succeeded = (cur_stat == kSucceeded);
  for (size_t i = 0; i < target_cnt_; ++i) {
    const auto detached = succeeded ? Complete(base_addr, targets_[i])  //
                                    : Abort(base_addr, targets_[i]);

    // the owner must detach its own embedding without any follower, and no
    // other embedding may remain in the targets it has not embedded
    const auto owned = (owned_mask & (1UL << i)) != 0;
    is_referred |= owned ? detached != (owned_addr | (i << kPosShift)) : detached != 0;
  }

  if (referred != nullptr) {
    *referred = is_referred;
  }
  return succeeded;
}

auto
MwCASDescriptor::DetermineThreadId(  //
    const size_t pos,                //
    const uint64_t word)             //
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
    if (target.addr->compare_exchange_weak(cur, target.old_val, kRelaxed, kRelaxed)) break;
    MWCAS_STAT_INC(kDetachRetry);
    CPP_UTILITY_SPINLOCK_HINT
  }
  return false;
}

/*############################################################################*
 * Internal classes
 *############################################################################*/

MwCASDescriptor::CompletedDescriptors::~CompletedDescriptors()  //
{
  while (!desc_deq_.empty()) {
    PassExpiredToGC();
    std::this_thread::sleep_for(std::chrono::milliseconds{dbgroup::memory::kDefaultGCTime});
  }
}

void
MwCASDescriptor::CompletedDescriptors::Retire(  //
    MwCASDescriptor* const desc)
{
  if (desc_deq_.size() >= kMaxReusableDescriptors) {
    PassExpiredToGC();
  }
  desc_deq_.emplace_back(desc, _gc->GetCurrentEpoch());
  MWCAS_STAT_INC(kDescDeferred);
}

void
MwCASDescriptor::CompletedDescriptors::PassExpiredToGC()
{
  // all the threads that could have embedded or referred to these descriptors
  // before their completion have left, and every stale embedding has been
  // detached by its creator; the GC protects threads that have referred to the
  // descriptors after their completion
  const auto min_epoch = _gc->GetMinEpoch();
  while (!desc_deq_.empty() && desc_deq_.front().second < min_epoch) {
    _gc->AddGarbage<MwCASDescriptor>(desc_deq_.front().first);
    desc_deq_.pop_front();
  }
}

}  // namespace dbgroup::atomic::mwcas::lock_free
