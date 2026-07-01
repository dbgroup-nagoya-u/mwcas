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

// external C++ libraries
#include <dbgroup/lock/common.hpp>
#include <dbgroup/memory/epoch_based_gc.hpp>

// local sources
#include "dbgroup/atomic/mwcas/utility.hpp"

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
    // phase 1: serialize MwCAS operations by embedding a descriptor
    auto mwcas_success = true;
    for (size_t i = begin_pos; i < target_cnt_; ++i) {
    retry_entry:
      auto cur = RDCSS(i, casn_base);
      if ((cur & kMwCASFlag) > 0 && cur != (casn_base | (i << kCntPos))) {
        auto* const desc = std::bit_cast<CASNDescriptor*>(cur & kPtrMask);
        desc->MwCASInternal(((cur & kCntMask) >> kCntPos) + 1);
        CPP_UTILITY_SPINLOCK_HINT
        goto retry_entry;  // NOLINT
      }
      if (cur != targets_[i].old_val) {
        mwcas_success = false;
        break;
      }
    }
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
      auto expected = target.addr->load(kRelaxed);
      if (expected != (casn_base | (i << kCntPos))) continue;
      target.addr->compare_exchange_strong(expected, target.new_val, kRelaxed, kRelaxed);
    }
  } else {
    for (size_t i = 0; i < target_cnt_; ++i) {
      auto& target = targets_[i];
      auto expected = target.addr->load(kRelaxed);
      if (expected != (casn_base | (i << kCntPos))) continue;
      target.addr->compare_exchange_strong(expected, target.old_val, kRelaxed, kRelaxed);
    }
  }

  return succeeded;
}

auto
CASNDescriptor::RDCSS(  //
    const size_t pos,
    const uint64_t casn_base)  //
    -> uint64_t
{
  const auto pos_bit = (pos << kCntPos);
  auto rdcss_addr = (casn_base ^ kFlagSwap) | pos_bit;
  auto& target = targets_[pos];
  auto cur = target.addr->load(kRelaxed);
  while (true) {
    if (cur & kRDCSSFlag) {
      CompleteRDCSS(cur);
      continue;
    }
    if (cur != target.old_val) return cur;
    if (target.addr->compare_exchange_strong(cur, rdcss_addr, kRelaxed, kRelaxed)) break;
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
CASNDescriptor::CompleteRDCSS(  //
    uint64_t& rdcss_addr)
{
  const auto casn_addr = rdcss_addr ^ kFlagSwap;
  auto* const desc = std::bit_cast<CASNDescriptor*>(rdcss_addr & kPtrMask);
  auto& target = desc->targets_[(rdcss_addr & kCntMask) >> kCntPos];

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
