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
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <utility>

// external libraries
#include "dbgroup/lock/common.hpp"
#include "dbgroup/memory/epoch_based_gc.hpp"

// local sources
#include "dbgroup/atomic/mwcas/utility.hpp"

namespace dbgroup::atomic::mwcas::lock_free
{

/*##############################################################################
 * Static utilities
 *############################################################################*/

void
AOPTDescriptor::StartGC(  //
    const size_t gc_interval,
    const size_t gc_thread_num)
{
  _gc = std::make_unique<EpochBasedGC_t>(gc_interval, gc_thread_num, kMaxReusableDescriptors);
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
    -> AOPTDescriptor *
{
  auto *page = _gc->GetPageIfPossible<AOPTDescriptor>();
  if (page == nullptr) return new AOPTDescriptor{};
  return static_cast<AOPTDescriptor *>(page);
}

/*##############################################################################
 * Utilities
 *############################################################################*/

auto
AOPTDescriptor::MwCAS()  //
    -> bool
{
  thread_local CompletedDescriptors completed_descriptors{};

  // serialize MwCAS operations by embedding a descriptor
  auto mwcas_success = true;
  for (size_t i = 0; i < target_count_; ++i) {
    auto &word_desc = targets_[i];
    const auto desc_addr = std::bit_cast<uint64_t>(&word_desc) | kMwCASFlag;

  retry_word:
    auto [cur, value] = ReadInternal(word_desc.addr, this, kRelaxed);
    if (cur == desc_addr) {
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

auto
AOPTDescriptor::ReadInternal(  //
    const std::atomic_uint64_t *addr,
    const AOPTDescriptor *self,
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
    auto *word_desc = std::bit_cast<WordDescriptor *>(word ^ kMwCASFlag);
    auto *aopt_desc = word_desc->parent;
    const auto stat = aopt_desc->stat_.load(kRelaxed);
    if (aopt_desc == self || stat != kActive) {
      value = (stat != kSuccessful) ? word_desc->old_val : word_desc->new_val;
      break;
    }

    // found the incomplete MwCAS
    aopt_desc->MwCAS();
  }

  return {word, value};
}

/*##############################################################################
 * Internal classes
 *############################################################################*/

AOPTDescriptor::CompletedDescriptors::~CompletedDescriptors()  //
{
  FinalizeCompletedDescriptors();
}

void
AOPTDescriptor::CompletedDescriptors::RetireForCleanUp(  //
    AOPTDescriptor *desc)
{
  if (desc_num_ >= kMaxReusableDescriptors) {
    FinalizeCompletedDescriptors();
  }
  desc_arr_[desc_num_++] = desc;
}

void
AOPTDescriptor::CompletedDescriptors::FinalizeCompletedDescriptors()
{
  for (size_t i = 0; i < desc_num_; ++i) {
    auto *desc = desc_arr_[i];
    const auto desc_addr = std::bit_cast<uint64_t>(desc) | kMwCASFlag;
    const auto target_num = desc->target_count_;
    if (desc->stat_.load(kRelaxed) == kSuccessful) {
      for (size_t i = 0; i < target_num; ++i) {
        auto &target = desc->targets_[i];
        auto cur = target.addr->load(kRelaxed);
        if (cur != desc_addr) continue;
        target.addr->compare_exchange_strong(cur, target.new_val, kRelaxed, kRelaxed);
      }
    } else {
      for (size_t i = 0; i < target_num; ++i) {
        auto &target = desc->targets_[i];
        auto cur = target.addr->load(kRelaxed);
        if (cur != desc_addr) continue;
        target.addr->compare_exchange_strong(cur, target.old_val, kRelaxed, kRelaxed);
      }
    }
    _gc->AddGarbage<AOPTDescriptor>(desc);
  }
  desc_num_ = 0;
}

}  // namespace dbgroup::atomic::mwcas::lock_free
