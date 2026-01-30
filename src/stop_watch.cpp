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

// the corresponding header
#include "dbgroup/benchmark/stop_watch.hpp"

// C++ standard libraries
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace dbgroup::benchmark
{
namespace
{
/*############################################################################*
 * Local constants
 *############################################################################*/

/// @brief A desired relative error.
constexpr double kAlpha = 0.01;

/// @brief The base value for approximation.
constexpr double kGamma = (1.0 + kAlpha) / (1.0 - kAlpha);

/// @brief The denominator for the logarithm change of base.
const double denom = std::log(kGamma);

}  // namespace

/*############################################################################*
 * Public APIs
 *############################################################################*/

void
StopWatch::Start() noexcept
{
  std::atomic_thread_fence(std::memory_order_acq_rel);
  st_ = Clock_t::now();
  std::atomic_thread_fence(std::memory_order_acq_rel);
}

void
StopWatch::Stop(  //
    const size_t cnt)
{
  using NS = std::chrono::nanoseconds;

  std::atomic_thread_fence(std::memory_order_acq_rel);
  const auto et = Clock_t::now();
  std::atomic_thread_fence(std::memory_order_acq_rel);

  const size_t lat = std::chrono::duration_cast<NS>(et - st_).count();
  exec_num_ += cnt;
  exec_time_ += lat;
  if (lat < min_) [[unlikely]] {
    min_ = lat;
  }
  if (lat > 0) [[likely]] {
    if (lat > max_) [[unlikely]] {
      max_ = lat;
    }
    const uint32_t pos = std::ceil(std::log(lat) / denom);
    ++bins_[pos];
  } else {
    ++bins_[0];
  }
}

auto
StopWatch::Quantile(       //
    const double q) const  //
    -> size_t
{
  if (q == 0) return min_;
  if (q >= 1.0) return max_;

  const auto bound = static_cast<uint32_t>(q * static_cast<double>(exec_num_ - 1));
  auto cnt = bins_[0];
  auto i = 0U;
  while (i < kBinNum - 1 && cnt <= bound) {
    cnt += bins_[++i];
  }
  return static_cast<size_t>(2 * std::pow(kGamma, i) / (kGamma + 1));
}

}  // namespace dbgroup::benchmark
