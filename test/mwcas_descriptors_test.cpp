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

// the corresponding headers
#include <dbgroup/atomic/mwcas/deadlock_free/mwcas_descriptor.hpp>
#include <dbgroup/atomic/mwcas/lock_free/aopt_descriptor.hpp>
#include <dbgroup/atomic/mwcas/lock_free/casn_descriptor.hpp>
#include <dbgroup/atomic/mwcas/lock_free/mwcas_descriptor.hpp>
#include <dbgroup/atomic/mwcas/lock_free/mwcas_descriptor_legacy.hpp>
#include <dbgroup/atomic/mwcas/lock_free/mwcas_descriptor_rc.hpp>
#include <dbgroup/atomic/mwcas/lock_free/mwcas_descriptor_weak.hpp>
#include <dbgroup/atomic/mwcas/obstruction_free/mwcas_descriptor.hpp>
#include <dbgroup/atomic/mwcas/obstruction_free/mwcas_descriptor_rc.hpp>

// C++ standard libraries
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

// external libraries
#include <gtest/gtest.h>

// external C++ libraries
#include <dbgroup/random/zipf.hpp>

// local sources
#include "common.hpp"

namespace dbgroup::atomic::mwcas::test
{
/*############################################################################*
 * Target MwCAS implementations
 *############################################################################*/

using DLFMwCAS = deadlock_free::MwCASDescriptor;
using LFMwCAS = lock_free::MwCASDescriptor;
using LFMwCASLegacy = lock_free::MwCASDescriptorLegacy;
using LFMwCASRC = lock_free::MwCASDescriptorRC;
using LFMwCASWeak = lock_free::MwCASDescriptorWeak;
using OFMwCAS = obstruction_free::MwCASDescriptor;
using OFMwCASRC = obstruction_free::MwCASDescriptorRC;
using CASN = lock_free::CASNDescriptor;
using AOPT = lock_free::AOPTDescriptor;

/*############################################################################*
 * Internal constants
 *############################################################################*/

constexpr size_t kExecNum = 1e6;

constexpr size_t kTargetFieldNum = kMwCASCapacity * kTestThreadNum;

constexpr double kSkewParameter = 0.0;

/// @brief An initial value for tests with decrements (to keep values positive).
constexpr uint64_t kInitValForDecrement = 1UL << 30UL;

/*############################################################################*
 * Fixture definitions
 *############################################################################*/

template <class MwCASDesc>
class MwCASDescriptorFixture : public ::testing::Test
{
 protected:
  /*##########################################################################*
   * Type aliases
   *##########################################################################*/

  using Target = uint64_t;
  using MwCASTargets = std::vector<std::pair<size_t, int64_t>>;  // (index, delta)
  using Zipf = ::dbgroup::random::ApproxZipfDistribution<uint64_t>;

  /*##########################################################################*
   * Internal constants
   *##########################################################################*/

  static constexpr size_t kOpsNum = std::is_same_v<MwCASDesc, CASN> ? 2e4 : kExecNum;

  /// @brief Implementations that use EBR for descriptors.
  static constexpr bool kUseEBR = std::is_same_v<MwCASDesc, AOPT>              //
                                  || std::is_same_v<MwCASDesc, CASN>           //
                                  || std::is_same_v<MwCASDesc, LFMwCAS>        //
                                  || std::is_same_v<MwCASDesc, LFMwCASLegacy>  //
                                  || std::is_same_v<MwCASDesc, OFMwCAS>        //
                                  || std::is_same_v<MwCASDesc, LFMwCASWeak>;

  /// @brief Implementations that use reference counting for descriptors.
  static constexpr bool kUseRC = std::is_same_v<MwCASDesc, LFMwCASRC>  //
                                 || std::is_same_v<MwCASDesc, OFMwCASRC>;

  /*##########################################################################*
   * Setup/Teardown
   *##########################################################################*/

  static void
  SetUpTestSuite()
  {
    dbgroup::thread::IDManager::SetMaxThreadNum(dbgroup::kMaxThreadCapacity);
  }

  void
  SetUp() override
  {
    for (size_t i = 0; i < kTargetFieldNum; ++i) {
      target_fields_[i] = 0UL;
    }

    if constexpr (kUseEBR) {
      MwCASDesc::StartGC();
    }
  }

  void
  TearDown() override
  {
    if constexpr (kUseEBR) {
      MwCASDesc::StopGC();
    }
  }

  /*##########################################################################*
   * Functions for verification
   *##########################################################################*/

  void
  VerifyMwCAS(  //
      const size_t thread_num)
  {
    with_decrement_ = false;
    RunMwCAS(thread_num);

    // check the target fields are correctly incremented
    EXPECT_EQ(kOpsNum * thread_num * kMwCASCapacity, SumTargets());
  }

  /**
   * @brief Verify MwCAS with random increments and decrements.
   *
   * Target fields can return to their old values (i.e., ABA), so stale helpers
   * can embed descriptors after their completion.
   */
  void
  VerifyMwCASWithDecrement(  //
      const size_t thread_num)
  {
    with_decrement_ = true;
    for (size_t i = 0; i < kTargetFieldNum; ++i) {
      target_fields_[i] = kInitValForDecrement;
    }
    delta_sum_ = 0;
    RunMwCAS(thread_num);

    // check the sum of target fields is equal to the sum of applied deltas
    const auto expected = static_cast<int64_t>(kInitValForDecrement * kTargetFieldNum)  //
                          + delta_sum_.load();
    EXPECT_EQ(expected, static_cast<int64_t>(SumTargets()));
  }

 private:
  /*##########################################################################*
   * Internal utility functions
   *##########################################################################*/

  auto
  SumTargets()  //
      -> size_t
  {
    size_t sum = 0;
    for (auto& target : target_fields_) {
      if constexpr (std::is_same_v<MwCASDesc, LFMwCASWeak>) {
        sum += MwCASDesc::template Read<Target>(&target).first;
      } else if constexpr (kUseEBR) {
        [[maybe_unused]] const auto& guard = MwCASDesc::CreateEpochGuard();
        sum += MwCASDesc::template Read<Target>(&target);
      } else {
        sum += MwCASDesc::template Read<Target>(&target);
      }
    }
    return sum;
  }

  void
  MwCAS(  //
      const MwCASTargets& targets)
  {
    if constexpr (std::is_same_v<MwCASDesc, DLFMwCAS>) {
      while (true) {
        MwCASDesc desc{};
        for (const auto& [idx, delta] : targets) {
          auto* const addr = &(target_fields_[idx]);
          const auto cur_val = MwCASDesc::template Read<Target>(addr, kRelaxed);
          const auto new_val = cur_val + delta;
          desc.AddMwCASTarget(addr, cur_val, new_val, kRelaxed);
        }
        if (desc.MwCAS()) return;
      }
    } else if constexpr (std::is_same_v<MwCASDesc, LFMwCASWeak>) {
      while (true) {
        [[maybe_unused]] const auto& guard = MwCASDesc::CreateEpochGuard();
        auto* const desc = MwCASDesc::GetDescriptor();
        for (const auto& [idx, delta] : targets) {
          auto* const addr = &(target_fields_[idx]);
          const auto [cur_val, word] = MwCASDesc::template Read<Target>(addr, kRelaxed);
          const auto new_val = cur_val + delta;
          desc->AddMwCASTarget(addr, word, new_val, kRelaxed);
        }
        if (desc->MwCAS()) return;
      }
    } else if constexpr (kUseRC) {
      while (true) {  // descriptors are protected by reference counts
        auto* const desc = MwCASDesc::GetDescriptor();
        for (const auto& [idx, delta] : targets) {
          auto* const addr = &(target_fields_[idx]);
          const auto cur_val = MwCASDesc::template Read<Target>(addr, kRelaxed);
          const auto new_val = cur_val + delta;
          desc->AddMwCASTarget(addr, cur_val, new_val, kRelaxed);
        }
        if (desc->MwCAS()) return;
      }
    } else {
      while (true) {
        [[maybe_unused]] const auto& guard = MwCASDesc::CreateEpochGuard();
        auto* const desc = MwCASDesc::GetDescriptor();
        for (const auto& [idx, delta] : targets) {
          auto* const addr = &(target_fields_[idx]);
          const auto cur_val = MwCASDesc::template Read<Target>(addr, kRelaxed);
          const auto new_val = cur_val + delta;
          desc->AddMwCASTarget(addr, cur_val, new_val, kRelaxed);
        }
        if (desc->MwCAS()) return;
      }
    }
  }

  void
  RunMwCAS(  //
      const size_t thread_num)
  {
    std::vector<std::thread> threads{};

    {  // create a lock to prevent workers from executing
      const std::unique_lock<std::shared_mutex> guard{worker_lock_};

      // run a function over multi-threads
      std::mt19937_64 rand_engine(kRandomSeed);  // NOLINT
      for (size_t i = 0; i < thread_num; ++i) {
        const auto rand_seed = rand_engine();
        threads.emplace_back(&MwCASDescriptorFixture::MwCASRandomly, this, rand_seed);
      }

      // wait for all workers to finish initialization
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      const std::unique_lock<std::shared_mutex> lock{main_lock_};
    }

    // wait for all workers to finish
    for (auto&& t : threads) t.join();
  }

  void
  MwCASRandomly(  //
      const size_t rand_seed)
  {
    std::vector<MwCASTargets> operations{};
    operations.reserve(kOpsNum);
    int64_t delta_sum = 0;

    {  // create a lock to prevent a main thread
      const std::shared_lock<std::shared_mutex> guard{main_lock_};

      // prepare operations to be executed
      std::mt19937_64 rand_engine{rand_seed};  // NOLINT
      std::bernoulli_distribution sign_dist{0.5};
      for (size_t i = 0; i < kOpsNum; ++i) {
        // select MwCAS target fields randomly
        MwCASTargets targets{};
        targets.reserve(kMwCASCapacity);
        while (targets.size() < kMwCASCapacity) {
          size_t idx = zipf_dist_(rand_engine);
          const auto iter = std::find_if(targets.begin(), targets.end(),
                                         [idx](const auto& t) { return t.first == idx; });
          if (iter == targets.end()) {
            const int64_t delta = (with_decrement_ && sign_dist(rand_engine)) ? -1 : 1;
            targets.emplace_back(idx, delta);
            delta_sum += delta;
          }
        }
        std::sort(targets.begin(), targets.end());

        // add a new targets
        operations.emplace_back(std::move(targets));
      }
    }

    {  // wait for a main thread to release a lock
      const std::shared_lock<std::shared_mutex> lock{worker_lock_};
      for (auto&& targets : operations) {
        MwCAS(targets);
      }
    }

    delta_sum_ += delta_sum;
  }

  /*##########################################################################*
   * Internal member variables
   *##########################################################################*/

  Target target_fields_[kTargetFieldNum]{};

  Zipf zipf_dist_{0, kTargetFieldNum - 1, kSkewParameter};

  std::shared_mutex main_lock_{};

  std::shared_mutex worker_lock_{};

  /// @brief A flag for using random decrements in addition to increments.
  bool with_decrement_{false};

  /// @brief The sum of all the applied deltas.
  std::atomic_int64_t delta_sum_{0};
};

/*############################################################################*
 * Preparation for typed testing
 *############################################################################*/

using MwCASDescriptors = ::testing::Types<DLFMwCAS,
                                          LFMwCAS,
                                          LFMwCASLegacy,
                                          LFMwCASRC,
                                          OFMwCAS,
                                          OFMwCASRC,
                                          LFMwCASWeak,
                                          AOPT,
                                          CASN>;
TYPED_TEST_SUITE(MwCASDescriptorFixture, MwCASDescriptors);

/*############################################################################*
 * Unit test definitions
 *############################################################################*/

TYPED_TEST(  //
    MwCASDescriptorFixture,
    MwCASWithSingleThreadCorrectlyIncrementTargets)
{
  TestFixture::VerifyMwCAS(1);
}

TYPED_TEST(  //
    MwCASDescriptorFixture,
    MwCASWithMultiThreadsCorrectlyIncrementTargets)
{
  TestFixture::VerifyMwCAS(kTestThreadNum);
}

TYPED_TEST(  //
    MwCASDescriptorFixture,
    MwCASWithMultiThreadsCorrectlyIncrementAndDecrementTargets)
{
  TestFixture::VerifyMwCASWithDecrement(kTestThreadNum);
}

}  // namespace dbgroup::atomic::mwcas::test
