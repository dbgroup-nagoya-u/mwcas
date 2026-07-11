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
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <utility>

// external C++ libraries
#include <dbgroup/lock/utility.hpp>
#include <dbgroup/memory/epoch_based_gc.hpp>

// local sources
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
constexpr uint64_t kThreadIdBitnum = 13;

/// @brief An offset to shift descriptor addresses (due to 64-byte alignment).
constexpr uint64_t kAddrAlignShift = 6;

/// @brief An offset for right-shifting to extract the "thread ID".
constexpr uint64_t kThreadIdShift = 41;

/// @brief An offset for right-shifting to extract the "reference counter".
constexpr uint64_t kCntShift = kThreadIdShift + kThreadIdBitnum;

/// @brief An offset for right-shifting to extract the "begin position".
constexpr uint64_t kPosShift = 60;

/// @brief A constant for incrementing the "reference counter".
constexpr uint64_t kCntUnit = 1UL << kCntShift;

/// @brief A bitmask with only the "descriptor address" portion set to 1.
constexpr uint64_t kAddrMask = (1UL << kThreadIdShift) - 1UL;

/// @brief A bitmask with only the "thread ID" portion set to 1.
constexpr uint64_t kThreadIdMask = ((1UL << kThreadIdBitnum) - 1UL) << kThreadIdShift;

/// @brief A bitmask with only the "reference counter" portion set to 1.
constexpr uint64_t kCntMask = ((1UL << (kPosShift - kCntShift)) - 1UL) << kCntShift;

/// @brief A bitmask with only the "begin position" portion set to 1.
constexpr uint64_t kPosMask = 7UL << kPosShift;

/// @brief A bitmask with only the "MwCAS FLAG" and "descriptor address" portions set to 1.
constexpr uint64_t kDescMask = kMwCASFlag | kAddrMask;

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
    desc = (page) ? static_cast<MwCASDescriptor*>(page) : new MwCASDescriptor{};
  }
  desc->target_cnt_ = 0;
  return desc;
}

auto
MwCASDescriptor::MwCAS()  //
    -> bool
{
  stat_.store(kUndecided, kRelease);  // set a memory fence
  const auto [succeeded, referred] = MwCASInternal();
  if (referred) {
    _gc->AddGarbage<MwCASDescriptor>(this);
  } else {
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
  const auto another_word = word;
  for (uint32_t i = 0; i < kRetryNum; ++i) {
    CPP_UTILITY_SPINLOCK_HINT
    word = addr->load(fence);
    if (word != another_word) return;
  }
  for (uint32_t i = 0; i < kRetryNum; ++i) {
    std::this_thread::yield();
    word = addr->load(fence);
    if (word != another_word) return;
  }

  const auto count = (word & kCntMask) >> kCntShift;
  std::this_thread::sleep_for(kBackOffTime * (1UL << count));  // exponential back-off

  word = addr->load(fence);
  if (word != another_word) return;  // other threads modified this field

  // a long CPU stall has been detected, so perform another MwCAS
  uint64_t incremented;
  if ((word & kCntMask) != kCntMask) [[likely]] {
    incremented = word;
  } else {
    incremented = word & ~kCntMask;
  }
  incremented += kCntMask;

  if (addr->compare_exchange_strong(word, incremented, kRelaxed, fence)) {
    // follow another MwCAS
    auto* const another_desc =
        std::bit_cast<MwCASDescriptor*>((word & kAddrMask) << kAddrAlignShift);
    const auto pos = (word & kPosMask) >> kPosShift;
    another_desc->MwCASInternal(pos);
    word = addr->load(fence);
  }
}

auto
MwCASDescriptor::Finalize(  //
    uint64_t desc_addr,     //
    MwCASTarget& target,    //
    bool succeeded)         //
    -> bool
{
  auto expected = target.addr->load(kRelaxed);
  while (true) {
    if (((expected ^ desc_addr) & kDescMask) != 0) return true;

    auto desired = target.old_val;
    if (succeeded) {
      const auto embedded_tid = (expected & kThreadIdMask) >> kThreadIdShift;
      // validation of thread ID
      if (embedded_tid == target.thread_id.load(kRelaxed)) {
        desired = target.new_val;
      }
    }

    if (target.addr->compare_exchange_weak(expected, desired, kRelaxed, kRelaxed)) {
      return (expected & kCntMask) != 0;
    }
    CPP_UTILITY_SPINLOCK_HINT
  }
}

auto
MwCASDescriptor::MwCASInternal(  //
    const size_t begin_pos)      //
    -> std::pair<bool, bool>
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
      const auto fence = target.fence;
      auto word = addr->load(kRelaxed);

      // try to embed the descriptor
      if (word == expected && addr->compare_exchange_strong(word, desc_addr, fence, kRelaxed)) {
        word = desc_addr;
      }

      // check another thread has embedded the descriptor
      if ((word & kDescMask) == base_addr) {
        const auto embedded_tid = (word & kThreadIdMask) >> kThreadIdShift;

        if (target.thread_id.load(kRelaxed) == kInitialThreadId) {
          auto expected_tid = kInitialThreadId;
          while (!target.thread_id.compare_exchange_weak(expected_tid, embedded_tid, kRelaxed,
                                                         kRelaxed)) {
            if (expected_tid != kInitialThreadId) {
              break;
            }
            CPP_UTILITY_SPINLOCK_HINT
          }
        }

        continue;
      }

      stat = kFailed;
      break;
    }

    // set a linearization point
    cur_stat = stat_.load(kRelaxed);
    if (cur_stat == kUndecided
        && stat_.compare_exchange_strong(cur_stat, stat, kRelaxed, kRelaxed)) {
      cur_stat = stat;
    }
  }

  const auto succeeded = (cur_stat == kSucceeded);
  bool referred = false;
  for (size_t i = 0; i < target_cnt_; ++i) {
    referred = Finalize(base_addr, targets_[i], succeeded) || referred;
  }

  return std::pair{succeeded, referred};
}

}  // namespace dbgroup::atomic::mwcas::lock_free
