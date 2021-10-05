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

#include "mwcas/mwcas_descriptor.hpp"

#include <gtest/gtest.h>

#include <future>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <thread>
#include <utility>
#include <vector>

#include "common.hpp"

namespace dbgroup::atomic::mwcas::test
{
class MwCASDescriptorFixture : public ::testing::Test
{
 protected:
  /*################################################################################################
   * Setup/Teardown
   *##############################################################################################*/

  void
  SetUp() override
  {
    for (size_t i = 0; i < kTargetFieldNum; ++i) {
      target_fields[i] = 0UL;
    }
  }

  void
  TearDown() override
  {
  }

  /*################################################################################################
   * Functions for verification
   *##############################################################################################*/

  void
  VerifyMwCAS(const size_t thread_num)
  {
    RunMwCAS(thread_num);

    // check the target fields are correctly incremented
    size_t sum = 0;
    for (auto &&target : target_fields) {
      sum += target;
    }

    EXPECT_EQ(kExecNum * thread_num * kMwCASCapacity, sum);
  }

 private:
  /*################################################################################################
   * Internal constants
   *##############################################################################################*/

  static constexpr size_t kExecNum = 1e6;
  static constexpr size_t kTargetFieldNum = kMwCASCapacity * kThreadNum;
  static constexpr size_t kRandomSeed = 20;

  /*################################################################################################
   * Internal type aliases
   *##############################################################################################*/

  using Target = uint64_t;
  using MwCASTargets = std::vector<size_t>;

  /*################################################################################################
   * Internal utility functions
   *##############################################################################################*/

  void
  RunMwCAS(const size_t thread_num)
  {
    std::vector<std::thread> threads;

    {  // create a lock to prevent workers from executing
      const std::unique_lock<std::shared_mutex> guard{worker_lock};

      // run a function over multi-threads
      std::mt19937_64 rand_engine(kRandomSeed);
      for (size_t i = 0; i < thread_num; ++i) {
        const auto rand_seed = rand_engine();
        threads.emplace_back(&MwCASDescriptorFixture::MwCASRandomly, this, rand_seed);
      }

      // wait for all workers to finish initialization
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      const std::unique_lock<std::shared_mutex> lock{main_lock};
    }

    // wait for all workers to finish
    for (auto &&t : threads) t.join();
  }

  void
  MwCASRandomly(const size_t rand_seed)
  {
    std::vector<MwCASTargets> operations;
    operations.reserve(kExecNum);

    {  // create a lock to prevent a main thread
      const std::shared_lock<std::shared_mutex> guard{main_lock};

      // prepare operations to be executed
      std::mt19937_64 rand_engine{rand_seed};
      for (size_t i = 0; i < kExecNum; ++i) {
        // select MwCAS target fields randomly
        MwCASTargets targets;
        targets.reserve(kMwCASCapacity);
        while (targets.size() < kMwCASCapacity) {
          size_t idx = id_dist(rand_engine);
          const auto iter = std::find(targets.begin(), targets.end(), idx);
          if (iter == targets.end()) {
            targets.emplace_back(std::move(idx));
          }
        }
        std::sort(targets.begin(), targets.end());

        // add a new targets
        operations.emplace_back(std::move(targets));
      }
    }

    {  // wait for a main thread to release a lock
      const std::shared_lock<std::shared_mutex> lock{worker_lock};

      for (auto &&targets : operations) {
        // retry until MwCAS succeeds
        while (true) {
          // register MwCAS targets
          MwCASDescriptor desc{};
          for (auto &&idx : targets) {
            auto addr = &(target_fields[idx]);
            const auto cur_val = ReadMwCASField<Target>(addr);
            const auto new_val = cur_val + 1;
            desc.AddMwCASTarget(addr, cur_val, new_val);
          }

          // perform MwCAS
          if (desc.MwCAS()) break;
        }
      }
    }
  }

  /*################################################################################################
   * Internal member variables
   *##############################################################################################*/

  Target target_fields[kTargetFieldNum];

  std::uniform_int_distribution<size_t> id_dist{0, kMwCASCapacity - 1};

  std::shared_mutex main_lock;
  std::shared_mutex worker_lock;
};

/*--------------------------------------------------------------------------------------------------
 * Public utility tests
 *------------------------------------------------------------------------------------------------*/

TEST_F(MwCASDescriptorFixture, MwCAS_SingleThread_CorrectlyIncrementTargets)
{  //
  VerifyMwCAS(1);
}

TEST_F(MwCASDescriptorFixture, MwCAS_MultiThreads_CorrectlyIncrementTargets)
{
  VerifyMwCAS(kThreadNum);
}

}  // namespace dbgroup::atomic::mwcas::test
