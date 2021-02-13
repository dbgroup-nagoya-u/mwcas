// Copyright (c) Database Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>

namespace dbgroup::atomic::mwcas
{
/// this library assumes that the length of one word is 8 bytes
constexpr size_t kWordSize = 8;

/// this library assumes that the size of one cache line is 64 bytes
constexpr size_t kCacheLineSize = 64;

/**
 * @brief An union to convert MwCAS target data into uint64_t.
 *
 * @tparam T a type of target data.
 */
template <class T>
union CASTargetConverter {
  static_assert(sizeof(T) == kWordSize);
  static_assert(std::is_trivially_copyable<T>::value == true);

  const T target_data;
  const uintptr_t converted_data;

  explicit CASTargetConverter(const T target) : target_data{target} {};

#if T != uintptr_t
  explicit CASTargetConverter(const uintptr_t converted) : converted_data{converted} {};
#endif
};

}  // namespace dbgroup::atomic::mwcas
