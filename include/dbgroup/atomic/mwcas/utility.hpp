/*
 * Copyright 2021 Database Group, Nagoya University
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

#ifndef DBGROUP_ATOMIC_MWCAS_UTILITY_HPP_
#define DBGROUP_ATOMIC_MWCAS_UTILITY_HPP_

// C++ standard libraries
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace dbgroup::atomic::mwcas
{
/*##############################################################################
 * Global enum and constants
 *############################################################################*/

/// @brief An alias of the acquire memory order.
constexpr std::memory_order kAcquire = std::memory_order_acquire;

/// @brief An alias of the release memory order.
constexpr std::memory_order kRelease = std::memory_order_release;

/// @brief An alias of the relaxed memory order.
constexpr std::memory_order kRelaxed = std::memory_order_relaxed;

/// @brief The last significant bit indicates MwCAS descriptors.
constexpr uint64_t kMwCASFlag = 1UL << 63UL;

/*##############################################################################
 * Tuning parameters
 *############################################################################*/

/// @brief Assumes that the size of one cache line is 64 bytes.
constexpr size_t kCacheLineSize = 64;

/// @brief The maximum number of target words of MwCAS.
constexpr size_t kMwCASCapacity = (MWCAS_CAPACITY);

/// @brief The maximum number of retries for preventing busy loops.
constexpr size_t kRetryNum = (MWCAS_RETRY_THRESHOLD);

/// @brief A sleep time for preventing busy loops [us].
constexpr std::chrono::microseconds kBackOffTime{MWCAS_BACKOFF_TIME};

/*##############################################################################
 * Global utility functions
 *############################################################################*/

/**
 * @tparam T A MwCAS target class.
 * @retval true if a target class can be updated by MwCAS.
 * @retval false otherwise.
 * @note Specialize this function to swap your own classes by MwCAS.
 */
template <class T>
constexpr auto
CanMwCAS()  //
    -> bool
{
  return std::is_same_v<T, uint64_t> || std::is_pointer_v<T>;
}

}  // namespace dbgroup::atomic::mwcas

#endif  // DBGROUP_ATOMIC_MWCAS_UTILITY_HPP_
