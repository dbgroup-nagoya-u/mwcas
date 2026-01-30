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

#ifndef DBGROUP_BENCHMARK_STOP_WATCH_HPP_
#define DBGROUP_BENCHMARK_STOP_WATCH_HPP_

// C++ standard libraries
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace dbgroup::benchmark
{
/**
 * @brief A class for computing approximated quantile.
 *
 * This implementation is based on DDSketch [1] but is simplified. This uses the
 * fixed number of bins and ignores the performance of quantile queries.
 *
 * [1] Charles Masson et al., "DDSketch: A fast and fully-mergeable quantile
 * sketch with relative-error guarantees," PVLDB, Vol. 12, No. 12, pp. 2195-2205,
 * 2019.
 */
class StopWatch
{
  /*##########################################################################*
   * Type aliases
   *##########################################################################*/

  using Clock_t = ::std::chrono::high_resolution_clock;

 public:
  /*##########################################################################*
   * Public constructors and assignment operators
   *##########################################################################*/

  constexpr StopWatch() noexcept = default;

  constexpr StopWatch(const StopWatch &) noexcept = default;
  constexpr StopWatch(StopWatch &&) noexcept = default;

  constexpr auto operator=(const StopWatch &obj) noexcept -> StopWatch & = default;
  constexpr auto operator=(StopWatch &&) noexcept -> StopWatch & = default;

  /*##########################################################################*
   * Public destructors
   *##########################################################################*/

  ~StopWatch() = default;

  /*##########################################################################*
   * Public operators
   *##########################################################################*/

  /**
   * @retval true if this stop watch has measurements.
   * @retval false otherwise.
   */
  [[nodiscard]]
  constexpr explicit
  operator bool() const noexcept
  {
    return exec_num_ > 0;
  }

  /**
   * @brief Merge a given sketch into this.
   *
   * @param rhs A sketch to be merged.
   */
  constexpr void
  operator+=(  //
      const StopWatch &rhs) noexcept
  {
    exec_num_ += rhs.exec_num_;
    exec_time_ += rhs.exec_time_;
    min_ = std::min(min_, rhs.min_);
    max_ = std::max(max_, rhs.max_);
    for (uint32_t i = 0; i < kBinNum; ++i) {
      bins_[i] += rhs.bins_[i];
    }
  }

  /*##########################################################################*
   * Public APIs
   *##########################################################################*/

  /**
   * @brief Start this stopwatch to measure time duration.
   *
   * @note If calling this function without the corresponding `Stop` function,
   * the beginning time of a measurement will be overwritten.
   */
  void Start() noexcept;

  /**
   * @brief Stop this stopwatch.
   *
   * @param cnt The number of executed operations for computing throughput.
   * @note The `Start` function must precede this every time. If calling this
   * function multiple times without the corresponding` Start`, the logged
   * execution time will be doubly counted.
   */
  void Stop(  //
      size_t cnt = 1);

  /**
   * @param q A target quantile value.
   * @return The latency of given quantile.
   */
  [[nodiscard]]
  auto Quantile(       //
      double q) const  //
      -> size_t;

  /**
   * @return The total execution time in nanoseconds.
   */
  [[nodiscard]]
  constexpr auto
  GetExecTime() const noexcept  //
      -> size_t
  {
    return exec_time_;
  }

  /**
   * @return The total number of executed operations.
   */
  [[nodiscard]]
  constexpr auto
  GetExecNum() const noexcept  //
      -> size_t
  {
    return exec_num_;
  }

 private:
  /*##########################################################################*
   * Internal constants
   *##########################################################################*/

  /// @brief The number of bins.
  static constexpr uint32_t kBinNum = 2048;

  /*##########################################################################*
   * Internal member variables
   *##########################################################################*/

  /// @brief A starting timestamp.
  Clock_t::time_point st_{};

  /// @brief The number of executed operations.
  uint32_t exec_num_{};

  /// @brief Total execution time [ns].
  size_t exec_time_{};

  /// @brief The minimum latency [ns].
  size_t min_{~0UL};

  /// @brief The maximum latency [ns].
  size_t max_{};

  /// @brief Execution time for each operation [ns].
  std::array<uint32_t, kBinNum> bins_{};
};

}  // namespace dbgroup::benchmark

#endif  // DBGROUP_BENCHMARK_STOP_WATCH_HPP_
