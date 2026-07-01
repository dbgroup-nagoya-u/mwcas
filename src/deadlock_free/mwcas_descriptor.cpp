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
#include "dbgroup/atomic/mwcas/deadlock_free/mwcas_descriptor.hpp"

// C++ standard libraries
#include <atomic>
#include <cstddef>

// external C++ libraries
#include <dbgroup/lock/common.hpp>

// local sources
#include "dbgroup/atomic/mwcas/utility.hpp"

namespace dbgroup::atomic::mwcas::deadlock_free
{

auto
MwCASDescriptor::MwCAS()  //
    -> bool
{
  // serialize MwCAS operations by embedding a descriptor
  const auto desc_addr = std::bit_cast<uint64_t>(this) | kMwCASFlag;
  auto mwcas_success = true;
  size_t embedded_count = 0;
  for (size_t i = 0; i < target_cnt_; ++i, ++embedded_count) {
    if (!EmbedDescriptor(desc_addr, i)) {
      mwcas_success = false;
      break;
    }
  }

  // complete MwCAS
  if (mwcas_success) {
    for (size_t i = 0; i < embedded_count; ++i) {
      auto& target = targets_[i];
      target.addr->store(target.new_val, kRelaxed);
    }
  } else {
    for (size_t i = 0; i < embedded_count; ++i) {
      auto& target = targets_[i];
      target.addr->store(target.old_val, kRelaxed);
    }
  }

  return mwcas_success;
}

auto
MwCASDescriptor::EmbedDescriptor(  //
    const uint64_t desc_addr,
    const size_t pos)  //
    -> bool
{
  auto& target = targets_[pos];
  auto* const addr = target.addr;
  const auto old_val = target.old_val;
  const auto fence = target.fence;

  for (size_t i = 1; true; ++i) {
    auto expected = addr->load(kRelaxed);
    if (expected == old_val
        && addr->compare_exchange_strong(expected, desc_addr, fence, kRelaxed)) {
      return true;
    }
    if ((expected & kMwCASFlag) == 0 || i >= kRetryNum) break;
    CPP_UTILITY_SPINLOCK_HINT
  }
  return false;
}

}  // namespace dbgroup::atomic::mwcas::deadlock_free
