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

#ifndef MWCAS_COMPONENT_COMMON_HPP
#define MWCAS_COMPONENT_COMMON_HPP

#ifdef MWCAS_HAS_SPINLOCK_HINT
#include <xmmintrin.h>
#define MWCAS_SPINLOCK_HINT _mm_pause();  // NOLINT
#else
#define MWCAS_SPINLOCK_HINT /* do nothing */
#endif

// C++ standard libraries
#include <atomic>

#include "mwcas/utility.hpp"

namespace dbgroup::atomic::mwcas::component
{
/*######################################################################################
 * Global enum and constants
 *####################################################################################*/

/// Assumes that the length of one word is 8 bytes
constexpr size_t kWordSize = 8;

/// Assumes that the size of one cache line is 64 bytes
constexpr size_t kCacheLineSize = 64;

}  // namespace dbgroup::atomic::mwcas::component

#endif  // MWCAS_COMPONENT_COMMON_HPP
