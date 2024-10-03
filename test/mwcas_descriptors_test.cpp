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
#include "dbgroup/atomic/mwcas/deadlock_free/mwcas_descriptor.hpp"
#include "dbgroup/atomic/mwcas/lock_free/aopt_descriptor.hpp"

// C++ standard libraries
#include <cstddef>
#include <future>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

// external libraries
#include "gtest/gtest.h"

// local sources
#include "common.hpp"

namespace dbgroup::atomic::mwcas::test
{
/*##############################################################################
 * Target MwCAS implementations
 *############################################################################*/

using DLFMwCAS = deadlock_free::MwCASDescriptor;
using AOPT = lock_free::AOPTDescriptor;

/*##############################################################################
 * Internal constants
 *############################################################################*/

constexpr size_t kExecNum = 1e6;

constexpr size_t kTargetFieldNum = kMwCASCapacity * kTestThreadNum;

/*##############################################################################
 * Fixture definitions
 *############################################################################*/

template <class MwCASDesc>
class MwCASDescriptorFixture : public ::testing::Test
{
 protected:
  /*############################################################################
   * Type aliases
   *##########################################################################*/

  using Target = uint64_t;
  using MwCASTargets = std::vector<size_t>;

  /*############################################################################
   * Setup/Teardown
   *##########################################################################*/

  void
  SetUp() override
  {
    for (size_t i = 0; i < kTargetFieldNum; ++i) {
      target_fields_[i] = 0UL;
    }

    if constexpr (std::is_same_v<MwCASDesc, AOPT>) {
      MwCASDesc::StartGC();
    }
  }

  void
  TearDown() override
  {
    if constexpr (std::is_same_v<MwCASDesc, AOPT>) {
      MwCASDesc::StopGC();
    }
  }

  /*############################################################################
   * Functions for verification
   *##########################################################################*/

  void
  VerifyMwCAS(  //
      const size_t thread_num)
  {
    RunMwCAS(thread_num);

    // check the target fields are correctly incremented
    size_t sum = 0;
    for (auto &target : target_fields_) {
      sum += MwCASDesc::template Read<Target>(&target);
    }

    EXPECT_EQ(kExecNum * thread_num * kMwCASCapacity, sum);
  }

 private:
  /*############################################################################
   * Internal utility functions
   *##########################################################################*/

  void
  MwCAS(  //
      const MwCASTargets &targets)
  {
    if constexpr (std::is_same_v<MwCASDesc, DLFMwCAS>) {
      while (true) {
        MwCASDesc desc{};
        for (auto idx : targets) {
          auto *addr = &(target_fields_[idx]);
          const auto cur_val = MwCASDesc::template Read<Target>(addr);
          const auto new_val = cur_val + 1;
          desc.AddMwCASTarget(addr, cur_val, new_val);
        }
        if (desc.MwCAS()) return;
      }
    } else {
      [[maybe_unused]] const auto &guard = MwCASDesc::CreateEpochGuard();
      while (true) {
        auto *desc = MwCASDesc::GetDescriptor();
        for (auto idx : targets) {
          auto *addr = &(target_fields_[idx]);
          const auto cur_val = MwCASDesc::template Read<Target>(addr);
          const auto new_val = cur_val + 1;
          desc->AddMwCASTarget(addr, cur_val, new_val);
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
    for (auto &&t : threads) t.join();
  }

  void
  MwCASRandomly(  //
      const size_t rand_seed)
  {
    std::vector<MwCASTargets> operations{};
    operations.reserve(kExecNum);

    {  // create a lock to prevent a main thread
      const std::shared_lock<std::shared_mutex> guard{main_lock_};

      // prepare operations to be executed
      std::mt19937_64 rand_engine{rand_seed};  // NOLINT
      for (size_t i = 0; i < kExecNum; ++i) {
        // select MwCAS target fields randomly
        MwCASTargets targets{};
        targets.reserve(kMwCASCapacity);
        while (targets.size() < kMwCASCapacity) {
          size_t idx = id_dist_(rand_engine);
          const auto iter = std::find(targets.begin(), targets.end(), idx);
          if (iter == targets.end()) {
            targets.emplace_back(idx);
          }
        }
        std::sort(targets.begin(), targets.end());

        // add a new targets
        operations.emplace_back(std::move(targets));
      }
    }

    {  // wait for a main thread to release a lock
      const std::shared_lock<std::shared_mutex> lock{worker_lock_};
      for (auto &&targets : operations) {
        MwCAS(targets);
      }
    }
  }

  /*############################################################################
   * Internal member variables
   *##########################################################################*/

  Target target_fields_[kTargetFieldNum]{};

  std::uniform_int_distribution<size_t> id_dist_{0, kMwCASCapacity - 1};

  std::shared_mutex main_lock_{};

  std::shared_mutex worker_lock_{};
};

/*##############################################################################
 * Preparation for typed testing
 *############################################################################*/

using MwCASDesctiptors = ::testing::Types<DLFMwCAS, AOPT>;
TYPED_TEST_SUITE(MwCASDescriptorFixture, MwCASDesctiptors);

/*##############################################################################
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

}  // namespace dbgroup::atomic::mwcas::test
