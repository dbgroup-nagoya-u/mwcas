/*
 * Copyright 2025 Database Group, Nagoya University
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

#ifndef DBGROUP_ATOMIC_MWCAS_STATS_HPP_
#define DBGROUP_ATOMIC_MWCAS_STATS_HPP_

// C++ standard libraries
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <ostream>

namespace dbgroup::atomic::mwcas::stats
{
/*############################################################################*
 * Global enum and constants
 *############################################################################*/

/**
 * @brief Event counters for analyzing MwCAS implementations.
 *
 */
enum Counter : size_t {
  /// @brief A reader has decided to follow an embedded descriptor.
  kFollow = 0,
  /// @brief A reader has aborted an undecided MwCAS (obstruction-free MwCAS).
  kAbortByReader,
  /// @brief A descriptor has been detached from a word with a non-zero counter.
  kDetachWithCnt,
  /// @brief A CAS for detaching a descriptor has failed and been retried.
  kDetachRetry,
  /// @brief A reader has waited because of a saturated reference counter.
  kSaturationWait,
  /// @brief A descriptor has been allocated by `new`.
  kDescNew,
  /// @brief A descriptor has been registered with a deferred retirement list.
  kDescDeferred,
  /// @brief A MwCAS operation has been attempted.
  kMwCASAttempt,
  /// @brief A MwCAS operation has succeeded.
  kMwCASSuccess,
  /// @brief The number of counters.
  kCounterNum,
};

/// @brief The names of counters for output.
constexpr std::array<const char*, kCounterNum> kCounterNames = {
    "follow",          "abort_by_reader", "detach_with_cnt", "detach_retry",  "saturation_wait",
    "desc_new",        "desc_deferred",   "mwcas_attempt",   "mwcas_success",
};

#ifdef MWCAS_ENABLE_STATS

/*############################################################################*
 * Global variables
 *############################################################################*/

/// @brief Counters aggregated from finished threads.
inline std::array<std::atomic_uint64_t, kCounterNum> _global{};  // NOLINT

/**
 * @brief Thread local counters that are aggregated when a thread exits.
 *
 */
struct LocalCounters {
  ~LocalCounters()
  {
    for (size_t i = 0; i < kCounterNum; ++i) {
      _global[i].fetch_add(cnt[i], std::memory_order_relaxed);
    }
  }

  /// @brief Thread local counters.
  std::array<uint64_t, kCounterNum> cnt{};
};

/// @brief Thread local counters.
inline thread_local LocalCounters _local{};  // NOLINT

/*############################################################################*
 * Global utility functions
 *############################################################################*/

/**
 * @brief Increment a thread local counter.
 *
 * @param counter A target counter.
 */
inline void
Increment(  //
    const Counter counter)
{
  ++_local.cnt[counter];
}

/**
 * @brief Output the counters aggregated from finished threads and reset them.
 *
 * @param os An output stream.
 * @param label A label for the output.
 * @note Call this function after all the worker threads have been joined.
 */
inline void
DumpAndReset(  //
    std::ostream& os,
    const char* label)
{
  os << "[stats] " << label;
  for (size_t i = 0; i < kCounterNum; ++i) {
    os << " " << kCounterNames[i] << "=" << _global[i].exchange(0, std::memory_order_relaxed);
  }
  os << "\n";
}

#define MWCAS_STAT_INC(counter) \
  ::dbgroup::atomic::mwcas::stats::Increment(::dbgroup::atomic::mwcas::stats::counter)

#else

/**
 * @brief Do nothing if statistics are disabled.
 *
 */
inline void
DumpAndReset(  //
    [[maybe_unused]] std::ostream& os,
    [[maybe_unused]] const char* label)
{
}

#define MWCAS_STAT_INC(counter) /* do nothing */

#endif

}  // namespace dbgroup::atomic::mwcas::stats

#endif  // DBGROUP_ATOMIC_MWCAS_STATS_HPP_
