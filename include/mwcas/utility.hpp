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

#ifndef MWCAS_UTILITY_HPP
#define MWCAS_UTILITY_HPP

#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace dbgroup::atomic::mwcas
{
/*######################################################################################
 * Global enum and constants
 *####################################################################################*/

/// The maximum number of target words of MwCAS
constexpr size_t kMwCASCapacity = MWCAS_CAPACITY;

/// The maximum number of retries for preventing busy loops.
constexpr size_t kRetryNum = MWCAS_RETRY_THRESHOLD;

/// A sleep time for preventing busy loops [us].
static constexpr auto kShortSleep = std::chrono::microseconds{MWCAS_SLEEP_TIME};

/*######################################################################################
 * Global utility functions
 *####################################################################################*/

/**
 * @tparam T a MwCAS target class.
 * @retval true if a target class can be updated by MwCAS.
 * @retval false otherwise.
 */
template <class T>
constexpr auto
CanMwCAS()  //
    -> bool
{
  if constexpr (std::is_same_v<T, uint64_t> || std::is_pointer_v<T>) {
    return true;
  } else {
    return false;
  }
}

}  // namespace dbgroup::atomic::mwcas

#endif  // MWCAS_UTILITY_HPP
