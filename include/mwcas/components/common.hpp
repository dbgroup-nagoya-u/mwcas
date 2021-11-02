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

#ifndef MWCAS_MWCAS_COMPONENT_COMMON_H_
#define MWCAS_MWCAS_COMPONENT_COMMON_H_

#include "../utility.hpp"

namespace dbgroup::atomic::mwcas::component
{
/*##################################################################################################
 * Global enum and constants
 *################################################################################################*/

/// A short name of std::memory_order_relaxed
constexpr std::memory_order mo_relax = std::memory_order_relaxed;

/// Assumes that the length of one word is 8 bytes
constexpr size_t kWordSize = 8;

/// Assumes that the size of one cache line is 64 bytes
constexpr size_t kCacheLineSize = 64;

/*##################################################################################################
 * Global utility structs
 *################################################################################################*/

/**
 * @brief An union to convert MwCAS target data into uint64_t.
 *
 * @tparam T a type of target data
 */
template <class T>
union CASTargetConverter {
  const T target_data;
  const uint64_t converted_data;

  explicit constexpr CASTargetConverter(const uint64_t converted) : converted_data{converted} {}

  explicit constexpr CASTargetConverter(const T target) : target_data{target} {}
};

/**
 * @brief Specialization for unsigned long type.
 *
 */
template <>
union CASTargetConverter<uint64_t> {
  const uint64_t target_data;
  const uint64_t converted_data;

  explicit constexpr CASTargetConverter(const uint64_t target) : target_data{target} {}
};

}  // namespace dbgroup::atomic::mwcas::component

#endif  // MWCAS_MWCAS_COMPONENT_COMMON_H_
