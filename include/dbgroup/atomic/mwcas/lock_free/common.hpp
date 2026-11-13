/*
 * Copyright 2026 Database Group, Nagoya University
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

#ifndef DBGROUP_ATOMIC_MWCAS_LOCK_FREE_COMMON_HPP_
#define DBGROUP_ATOMIC_MWCAS_LOCK_FREE_COMMON_HPP_

// C++ standard libraries
#include <atomic>
#include <cstdint>
#include <thread>

// external C++ libraries
#include <dbgroup/constants.hpp>
#include <dbgroup/lock/utility.hpp>

// local sources
#include "dbgroup/atomic/mwcas/utility.hpp"

namespace dbgroup::atomic::mwcas::lock_free
{
/*############################################################################*
 * Internal utility functions
 *############################################################################*/

/**
 * @brief Insert back-off and register a follower if needed.
 *
 * @tparam kCntMask A bit mask for extracting a counter.
 * @tparam kCntShift A bit shift for extracting a counter.
 * @tparam kNeedReference A flag for preventing the reuse of referred descriptors.
 * @param[in] addr A target address.
 * @param[in,out] word The current value of the target address.
 * @param[in] fence A memory fence.
 * @retval true if the caller should follow the MwCAS.
 * @retval false otherwise.
 */
template <uint64_t kCntMask, uint64_t kCntShift, bool kNeedReference>
inline auto
PrepareToFollow(  //
    std::atomic_uint64_t* const addr,
    uint64_t& word,
    const std::memory_order fence)  //
    -> bool
{
  constexpr uint64_t kCntUnit = 1UL << kCntShift;
  if constexpr (!kUseBackOff) {
    // the counter is used only as a "referred" flag
    if (!kNeedReference || (word & kCntMask) != 0) return true;
    return addr->compare_exchange_strong(word, word + kCntUnit, kRelaxed, fence);
  } else {
    // wait for other threads to make progress on the incomplete MwCAS
    const auto expected = word;
    for (uint32_t i = 0; i < kRetryNum; ++i) {
      CPP_UTILITY_SPINLOCK_HINT
      word = addr->load(fence);
      if (word != expected) return false;
    }

    const auto count = (word & kCntMask) >> kCntShift;
    std::this_thread::sleep_for(kBackOffTime * (1UL << count));  // exponential back-off
    word = addr->load(fence);
    if (word != expected) return false;  // other threads modified this field

    // a long CPU stall has been detected, so increment the counter
    uint64_t incremented;
    if ((word & kCntMask) != kCntMask) [[likely]] {
      incremented = word;
    } else {
      incremented = word & ~kCntMask;
    }
    incremented += kCntUnit;
    return addr->compare_exchange_strong(word, incremented, kRelaxed, fence);
  }
}

}  // namespace dbgroup::atomic::mwcas::lock_free

#endif  // DBGROUP_ATOMIC_MWCAS_LOCK_FREE_COMMON_HPP_
