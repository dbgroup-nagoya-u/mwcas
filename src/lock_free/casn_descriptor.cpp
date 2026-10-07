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
#include "dbgroup/atomic/mwcas/lock_free/casn_descriptor.hpp"

// C++ standard libraries
#include <array>
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
#include "dbgroup/atomic/mwcas/utility.hpp"

//                           Bit allocation of a word.
// |     63     |     62     |       61-56       |   55-47  |        46-0        |
// | MwCAS Flag | RDCSS Flag | Reference Counter | Position | Descriptor Address |
//

namespace dbgroup::atomic::mwcas::lock_free
{
/*############################################################################*
 * Static utilities
 *############################################################################*/

void
CASNDescriptor::StartGC(  //
    const size_t gc_interval,
    const size_t gc_thread_num)
{
  _gc = std::make_unique<EpochBasedGC>(gc_interval, gc_thread_num, kMaxReusableDescriptors);
}

void
CASNDescriptor::StopGC()
{
  _gc.reset();
}

auto
CASNDescriptor::CreateEpochGuard()  //
    -> ::dbgroup::thread::EpochGuard
{
  return _gc->CreateEpochGuard();
}

auto
CASNDescriptor::GetDescriptor()  //
    -> CASNDescriptor*
{
  auto* const page = _gc->GetPageIfPossible<CASNDescriptor>();
  auto* const desc = (page == nullptr) ? new CASNDescriptor{} : static_cast<CASNDescriptor*>(page);
  desc->target_cnt_ = 0;
  return desc;
}

auto
CASNDescriptor::GetRDCSSDescriptor()  //
    -> RDCSSDescriptor*
{
  auto* const page = _gc->GetPageIfPossible<RDCSSDescriptor>();
  return (page == nullptr) ? new RDCSSDescriptor{} : static_cast<RDCSSDescriptor*>(page);
}

/*############################################################################*
 * Utilities
 *############################################################################*/

auto
CASNDescriptor::MwCAS()  //
    -> bool
{
  // set a memory fence
  stat_.store(kUndecided, kRelease);
  const auto succeeded = MwCASInternal();
  _gc->AddGarbage<CASNDescriptor>(this);
  return succeeded;
}

auto
CASNDescriptor::MwCASInternal(  // NOLINT
    const size_t begin_pos)     //
    -> bool
{
  const auto casn_base = std::bit_cast<uint64_t>(this) | kMwCASFlag;

  auto stat = stat_.load(kAcquire);
  if (stat == kUndecided) {
    // prepare a fresh RDCSS descriptor for this invocation
    auto* const rdcss = GetRDCSSDescriptor();
    rdcss->casn = this;
    const auto rdcss_base = std::bit_cast<uint64_t>(rdcss) | kRDCSSFlag;

    // phase 1: serialize MwCAS operations by embedding a descriptor
    auto mwcas_success = true;
    for (size_t i = begin_pos; i < target_cnt_; ++i) {
    retry_entry:
      auto cur = RDCSS(i, casn_base, rdcss_base);
      if ((cur & kMwCASFlag) > 0) {
        // this entry has already been embedded by helpers
        if ((cur & ~kRefCntMask) == (casn_base | (i << kCntPos))) continue;
        FollowIfNeeded(targets_[i].addr, cur, kRelaxed);
        CPP_UTILITY_SPINLOCK_HINT
        goto retry_entry;  // NOLINT
      }
      if (cur != targets_[i].old_val) {
        mwcas_success = false;
        break;
      }
    }

    // the RDCSS descriptor is no longer embedded in any target
    _gc->AddGarbage<RDCSSDescriptor>(rdcss);

    const auto desired = mwcas_success ? kSucceeded : kFailed;
    stat = stat_.load(kRelaxed);
    if (stat == kUndecided && stat_.compare_exchange_strong(stat, desired, kRelaxed, kRelaxed)) {
      stat = desired;
    }
  }

  // phase 2: complete this MwCAS operation
  const auto succeeded = stat == kSucceeded;
  if (succeeded) {
    for (size_t i = 0; i < target_cnt_; ++i) {
      auto& target = targets_[i];
      const auto casn_addr = casn_base | (i << kCntPos);
      auto expected = target.addr->load(kRelaxed);
      while ((expected & ~kRefCntMask) == casn_addr) {
        // retry if other threads have incremented the reference counter
        if (target.addr->compare_exchange_weak(expected, target.new_val, kRelaxed, kRelaxed)) break;
        CPP_UTILITY_SPINLOCK_HINT
      }
    }
  } else {
    for (size_t i = 0; i < target_cnt_; ++i) {
      auto& target = targets_[i];
      const auto casn_addr = casn_base | (i << kCntPos);
      auto expected = target.addr->load(kRelaxed);
      while ((expected & ~kRefCntMask) == casn_addr) {
        // retry if other threads have incremented the reference counter
        if (target.addr->compare_exchange_weak(expected, target.old_val, kRelaxed, kRelaxed)) break;
        CPP_UTILITY_SPINLOCK_HINT
      }
    }
  }

  return succeeded;
}

auto
CASNDescriptor::RDCSS(  //
    const size_t pos,
    const uint64_t casn_base,
    const uint64_t rdcss_base)  //
    -> uint64_t
{
  const auto pos_bit = (pos << kCntPos);
  auto rdcss_addr = rdcss_base | pos_bit;
  auto& target = targets_[pos];
  auto cur = target.addr->load(kRelaxed);
  while (true) {
    if (cur & kRDCSSFlag) {
      CompleteRDCSS(cur);
      continue;
    }
    if (cur != target.old_val) return cur;
    if (target.addr->compare_exchange_strong(cur, rdcss_addr, kRelease, kRelaxed)) break;
    CPP_UTILITY_SPINLOCK_HINT
  }

  // RDCSS embedding succeeded, so complete this RDCSS operation
  if (stat_.load(kAcquire) != kUndecided) {
    // CASN embedding has already finished
    target.addr->compare_exchange_strong(rdcss_addr, target.old_val, kRelaxed, kRelaxed);
  } else {
    target.addr->compare_exchange_strong(rdcss_addr, casn_base | pos_bit, target.fence, kRelaxed);
  }
  return target.old_val;
}

void
CASNDescriptor::FollowIfNeeded(  // NOLINT
    [[maybe_unused]] std::atomic_uint64_t* const addr,
    uint64_t word,
    [[maybe_unused]] const std::memory_order fence)
{
#ifdef MWCAS_USE_BACKOFF
  // wait for the incomplete MwCAS to be completed by its owner
  const auto another_word = word;
  for (uint32_t i = 0; i < kRetryNum && word == another_word; ++i) {
    CPP_UTILITY_SPINLOCK_HINT
    word = addr->load(fence);
  }
  if (word != another_word) return;  // other threads modified this field

  const auto count = (word & kRefCntMask) >> kRefCntShift;
  std::this_thread::sleep_for(kBackOffTime * (1UL << count));  // exponential back-off
  if (addr->load(fence) != another_word) return;               // other threads modified this field

  // a long CPU stall has been detected, so increment the reference counter
  uint64_t incremented;
  if ((word & kRefCntMask) != kRefCntMask) [[likely]] {
    incremented = word;
  } else {
    incremented = word & ~kRefCntMask;
  }
  incremented += kRefCntUnit;
  if (!addr->compare_exchange_strong(word, incremented, kRelaxed, fence)) return;
#endif

  // follow the incomplete MwCAS
  auto* const desc = std::bit_cast<CASNDescriptor*>(word & kPtrMask);
  desc->MwCASInternal(((word & kCntMask) >> kCntPos) + 1);
}

void
CASNDescriptor::CompleteRDCSS(  //
    uint64_t& rdcss_addr)
{
  // synchronize with the release CAS that installed the RDCSS descriptor
  std::atomic_thread_fence(kAcquire);

  const auto pos_bit = rdcss_addr & kCntMask;
  const auto* const rdcss = std::bit_cast<RDCSSDescriptor*>(rdcss_addr & kPtrMask);
  auto* const desc = rdcss->casn;
  const auto casn_addr = std::bit_cast<uint64_t>(desc) | kMwCASFlag | pos_bit;
  auto& target = desc->targets_[pos_bit >> kCntPos];

  if (desc->stat_.load(kAcquire) != kUndecided) {
    // CASN embedding has already finished
    if (target.addr->compare_exchange_strong(rdcss_addr, target.old_val, kRelaxed, kRelaxed)) {
      rdcss_addr = target.old_val;
      return;
    }
  } else if (target.addr->compare_exchange_strong(rdcss_addr, casn_addr, target.fence, kRelaxed)) {
    // CASN embedding succeeded
    rdcss_addr = casn_addr;
    return;
  }
  CPP_UTILITY_SPINLOCK_HINT
}

}  // namespace dbgroup::atomic::mwcas::lock_free
