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
#include "dbgroup/atomic/mwcas/lock_free/aopt_descriptor.hpp"

// C++ standard libraries
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <thread>
#include <utility>

// external C++ libraries
#include <dbgroup/lock/utility.hpp>
#include <dbgroup/memory/epoch_based_gc.hpp>

// local sources
#include "dbgroup/atomic/mwcas/utility.hpp"
#include "dbgroup/memory/utility.hpp"

//                    Bit allocation of a word.
// |     63     |       62-57       |   56-47  |        46-0        |
// | MwCAS Flag | Reference Counter | Position | Descriptor Address |
//

namespace dbgroup::atomic::mwcas::lock_free
{
namespace
{
/*############################################################################*
 * Local constants
 *############################################################################*/

/// @brief The bit position for indicating the original number of a target.
constexpr uint64_t kCntPos = 47;

/// @brief An offset for right-shifting to extract reference counter.
constexpr uint64_t kRefCntShift = 57;

/// @brief A constant for incrementing reference counter.
constexpr uint64_t kRefCntUnit = 1UL << kRefCntShift;

/// @brief A bit mask for extracting pointers.
constexpr uint64_t kPtrMask = (1UL << kCntPos) - 1UL;

/// @brief A bit mask for extracting the original number of a target.
constexpr uint64_t kCntMask = (kRefCntUnit - 1UL) ^ kPtrMask;

/// @brief A bit mask for extracting reference counter.
constexpr uint64_t kRefCntMask = (kMwCASFlag - 1UL) ^ (kCntMask | kPtrMask);

static_assert(kMwCASCapacity <= (kCntMask >> kCntPos) + 1);

}  // namespace

/*############################################################################*
 * Static utilities
 *############################################################################*/

void
AOPTDescriptor::StartGC(  //
    const size_t gc_interval,
    const size_t gc_thread_num)
{
  _gc = std::make_unique<EpochBasedGC>(gc_interval, gc_thread_num, kMaxReusableDescriptors);
}

void
AOPTDescriptor::StopGC()
{
  _gc.reset();
}

auto
AOPTDescriptor::CreateEpochGuard()  //
    -> ::dbgroup::thread::EpochGuard
{
  return _gc->CreateEpochGuard();
}

auto
AOPTDescriptor::GetDescriptor()  //
    -> AOPTDescriptor*
{
  auto* const page = _gc->GetPageIfPossible<AOPTDescriptor>();
  auto* const desc = (page == nullptr) ? new AOPTDescriptor{} : static_cast<AOPTDescriptor*>(page);
  desc->target_cnt_ = 0;
  return desc;
}

/*############################################################################*
 * Utilities
 *############################################################################*/

auto
AOPTDescriptor::MwCAS()  //
    -> bool
{
  // set a memory fence
  stat_.store(kActive, kRelease);
  return MwCASInternal();
}

auto
AOPTDescriptor::ReadInternal(  // NOLINT
    const std::atomic_uint64_t* const addr,
    const AOPTDescriptor* const self,
    const std::memory_order fence)  //
    -> std::pair<uint64_t, uint64_t>
{
  uint64_t word{};
  uint64_t value{};
  while (true) {
    word = addr->load(fence);
    if ((word & kMwCASFlag) == 0) {
      value = word;
      break;
    }

    // found a word descriptor
    auto* const desc = std::bit_cast<AOPTDescriptor*>(word & kPtrMask);
    const auto pos = (word & kCntMask) >> kCntPos;
    const auto stat = desc->stat_.load(kAcquire);
    if (desc == self || stat != kActive) {
      value = (stat != kSuccessful) ? desc->targets_[pos].old_val : desc->targets_[pos].new_val;
      break;
    }

#ifdef MWCAS_USE_BACKOFF
    // wait for the incomplete MwCAS to be completed by its owner
    const auto another_word = word;
    for (uint32_t i = 0; i < kRetryNum && word == another_word; ++i) {
      CPP_UTILITY_SPINLOCK_HINT
      word = addr->load(fence);
    }
    if (word != another_word) continue;  // other threads modified this field

    const auto count = (word & kRefCntMask) >> kRefCntShift;
    std::this_thread::sleep_for(kBackOffTime * (1UL << count));  // exponential back-off
    if (addr->load(fence) != another_word) continue;  // other threads modified this field

    // a long CPU stall has been detected, so increment the reference counter
    uint64_t incremented;
    if ((word & kRefCntMask) != kRefCntMask) [[likely]] {
      incremented = word;
    } else {
      incremented = word & ~kRefCntMask;
    }
    incremented += kRefCntUnit;
    auto* const mutable_addr = const_cast<std::atomic_uint64_t*>(addr);  // NOLINT
    if (!mutable_addr->compare_exchange_strong(word, incremented, kRelaxed, fence)) continue;
#endif

    // found the incomplete MwCAS
    desc->MwCASInternal(pos + 1);
    CPP_UTILITY_SPINLOCK_HINT
  }

  return {word, value};
}

auto
AOPTDescriptor::MwCASInternal(  // NOLINT
    const size_t begin_pos)     //
    -> bool
{
  thread_local CompletedDescriptors completed_descriptors{};
  const auto base_addr = std::bit_cast<uint64_t>(this) | kMwCASFlag;

  // serialize MwCAS operations by embedding a descriptor
  auto mwcas_success = true;
  for (size_t i = begin_pos; i < target_cnt_; ++i) {
    auto& word_desc = targets_[i];
    const auto desc_addr = base_addr | (i << kCntPos);

  retry_word:
    auto [cur, value] = ReadInternal(word_desc.addr, this, kRelaxed);
    if ((cur & ~kRefCntMask) == desc_addr) {
      // this word already points to the right place, move on
      continue;
    }

    if (value != word_desc.old_val) {
      // the expected value is different, the MwCAS fails
      mwcas_success = false;
      break;
    }

    if (stat_.load(kRelaxed) != kActive) {
      // this MwCAS has already completed
      break;
    }

    // try to install the pointer to my descriptor
    if (!word_desc.addr->compare_exchange_strong(cur, desc_addr, word_desc.fence, kRelaxed)) {
      CPP_UTILITY_SPINLOCK_HINT
      goto retry_word;  // NOLINT
    }
  }

  // update status of this descriptor
  auto expected = stat_.load(kRelaxed);
  if (expected == kActive) {
    const auto desired = (mwcas_success) ? kSuccessful : kFailed;
    if (stat_.compare_exchange_strong(expected, desired, kRelaxed, kRelaxed)) {
      // if this thread finalized the descriptor, mark it for reclamation
      completed_descriptors.RetireForCleanUp(this);
      expected = desired;
    }
  }
  return expected == kSuccessful;
}

/*############################################################################*
 * Internal classes
 *############################################################################*/

AOPTDescriptor::CompletedDescriptors::~CompletedDescriptors()  //
{
  while (!desc_deq_.empty()) {
    FinalizeCompletedDescriptors();
    std::this_thread::sleep_for(std::chrono::milliseconds{dbgroup::memory::kDefaultGCTime});
  }
}

void
AOPTDescriptor::CompletedDescriptors::RetireForCleanUp(  //
    AOPTDescriptor* const desc)
{
  if (desc_deq_.size() >= kMaxReusableDescriptors) {
    FinalizeCompletedDescriptors();
  }
  desc_deq_.emplace_back(desc, _gc->GetCurrentEpoch());
}

void
AOPTDescriptor::CompletedDescriptors::FinalizeCompletedDescriptors()
{
  const auto min_epoch = _gc->GetMinEpoch();
  while (!desc_deq_.empty() && desc_deq_.front().second < min_epoch) {
    auto* const desc = desc_deq_.front().first;
    desc_deq_.pop_front();
    const auto desc_addr = std::bit_cast<uint64_t>(desc) | kMwCASFlag;
    const auto target_num = desc->target_cnt_;
    if (desc->stat_.load(kRelaxed) == kSuccessful) {
      for (size_t i = 0; i < target_num; ++i) {
        auto& target = desc->targets_[i];
        const auto word_addr = desc_addr | (i << kCntPos);
        auto cur = target.addr->load(kRelaxed);
        while ((cur & ~kRefCntMask) == word_addr) {
          // retry if other threads have incremented the reference counter
          if (target.addr->compare_exchange_weak(cur, target.new_val, kRelaxed, kRelaxed)) break;
          CPP_UTILITY_SPINLOCK_HINT
        }
      }
    } else {
      for (size_t i = 0; i < target_num; ++i) {
        auto& target = desc->targets_[i];
        const auto word_addr = desc_addr | (i << kCntPos);
        auto cur = target.addr->load(kRelaxed);
        while ((cur & ~kRefCntMask) == word_addr) {
          // retry if other threads have incremented the reference counter
          if (target.addr->compare_exchange_weak(cur, target.old_val, kRelaxed, kRelaxed)) break;
          CPP_UTILITY_SPINLOCK_HINT
        }
      }
    }
    _gc->AddGarbage<AOPTDescriptor>(desc);
  }
}

}  // namespace dbgroup::atomic::mwcas::lock_free
