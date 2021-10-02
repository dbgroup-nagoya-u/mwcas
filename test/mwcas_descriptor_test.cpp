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
#include <thread>
#include <utility>
#include <vector>

namespace dbgroup::atomic::mwcas::test
{
using Target = uint64_t;

class MwCASDescriptorFixture : public ::testing::Test
{
 protected:
  static constexpr size_t kInnerLoopNum = 10000;
  static constexpr size_t kOuterLoopNum = 100;
  static constexpr size_t kInitVal = 999999;

#ifdef MWCAS_TEST_THREAD_NUM
  static constexpr size_t kThreadNum = MWCAS_TEST_THREAD_NUM;
#else
  static constexpr size_t kThreadNum = 8;
#endif

  Target targets[kMwCASCapacity];

  static void
  InitializeTargetArray(Target arr[kMwCASCapacity])
  {
    for (size_t i = 0; i < kMwCASCapacity; ++i) {
      arr[i] = kInitVal;
    }
  }

  void
  TestMwCAS(  //
      const size_t thread_id,
      const size_t target_num)
  {
    for (uint64_t count = 0; count < kInnerLoopNum; ++count) {
      const auto new_val = thread_id + kThreadNum * count;
      auto desc = MwCASDescriptor{};

      // prepare MwCAS targets
      Target old_values[kMwCASCapacity];
      InitializeTargetArray(old_values);
      for (size_t i = 0; i < target_num; ++i) {
        old_values[i] = ReadMwCASField<Target>(&targets[i]);
        desc.AddMwCASTarget(&targets[i], old_values[i], new_val);
      }

      // perform MwCAS
      const auto mwcas_success = desc.MwCAS();

      // read current values
      Target read_values[kMwCASCapacity];
      InitializeTargetArray(read_values);
      for (size_t i = 0; i < target_num; ++i) {
        read_values[i] = ReadMwCASField<Target>(&targets[i]);
      }

      // check whether read values are valid
      for (size_t i = 0; i < target_num; ++i) {
        const auto expected = (mwcas_success) ? new_val : old_values[i];
        // a read value is valid when
        // 1) it equals to an expected one or
        // 2) it is written by the other threads
        EXPECT_TRUE(read_values[i] == expected || read_values[i] % kThreadNum != thread_id);
      }
    }
  }

  void
  PerformMwCAS(  //
      const size_t target_num,
      const size_t thread_num)
  {
    assert(0 < target_num && target_num <= kMwCASCapacity);
    assert(0 < thread_num && thread_num <= kThreadNum);

    for (size_t count = 0; count < kOuterLoopNum; ++count) {
      // perform MwCAS tests with multi-threads
      std::vector<std::thread> threads;
      for (size_t id = 0; id < thread_num; ++id) {
        threads.emplace_back(std::thread{&MwCASDescriptorFixture::TestMwCAS, this, id, target_num});
      }
      for (auto &&thread : threads) {
        thread.join();
      }

      // check whether MwCAS target fields are atomically updated
      const auto expected = ReadMwCASField<Target>(&targets[0]);
      for (size_t i = 1; i < target_num; ++i) {
        const auto read_value = ReadMwCASField<Target>(&targets[i]);
        EXPECT_EQ(expected, read_value);
      }
    }
  }

  void
  SetUp() override
  {
    InitializeTargetArray(targets);
  }

  void
  TearDown() override
  {
  }
};

/*--------------------------------------------------------------------------------------------------
 * Public utility tests
 *------------------------------------------------------------------------------------------------*/

TEST_F(MwCASDescriptorFixture, MwCAS_OneFieldSingleThread_ReadValidValues)
{  //
  PerformMwCAS(1, 1);
}

TEST_F(MwCASDescriptorFixture, MwCAS_OneFieldMultiThreads_ReadValidValues)
{
  PerformMwCAS(1, kThreadNum);
}

TEST_F(MwCASDescriptorFixture, MwCAS_MultiFieldsSingleThread_ReadValidValues)
{
  PerformMwCAS(kMwCASCapacity, 1);
}

TEST_F(MwCASDescriptorFixture, MwCAS_MultiFieldsMultiThreads_ReadValidValues)
{
  PerformMwCAS(kMwCASCapacity, kThreadNum);
}

}  // namespace dbgroup::atomic::mwcas::test
