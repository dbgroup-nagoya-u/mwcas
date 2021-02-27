// Copyright (c) Database Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace dbgroup::atomic::mwcas
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

#ifdef MWCAS_CAPACITY
/// The maximum number of target words of MwCAS
constexpr size_t kMwCASCapacity = MWCAS_CAPACITY;
#else
/// The maximum number of target words of MwCAS
constexpr size_t kMwCASCapacity = 4;
#endif

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
  static_assert(sizeof(T) == kWordSize);
  static_assert(std::is_trivially_copyable<T>::value == true);

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

}  // namespace dbgroup::atomic::mwcas
