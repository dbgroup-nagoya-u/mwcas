// Copyright (c) DB Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#include "mwcas_manager.hpp"

#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "mwcas_entry.hpp"

namespace dbgroup::atomic::mwcas
{
class MwCASManagerFixture : public ::testing::Test
{
 public:
  MwCASManager manager{1};

 protected:
  void
  SetUp() override
  {
  }

  void
  TearDown() override
  {
  }
};

/*--------------------------------------------------------------------------------------------------
 * Public utility tests
 *------------------------------------------------------------------------------------------------*/

TEST_F(MwCASManagerFixture, MwCAS_OneFieldSingleThread_ReadValidValues)
{
  constexpr auto kLoopNum = 1000UL;
  constexpr auto kThreadNum = 1UL;

  const auto init = 9999UL;
  auto target = uint64_t{init};

  auto f = [&manager = manager, &target = target](const uint64_t begin_index) {
    for (uint64_t count = 0; count < kLoopNum; ++count) {
      std::vector<MwCASEntry> entries;

      const auto old_val = MwCASManager::ReadMwCASField<uint64_t>(&target);
      const auto new_val = begin_index + kThreadNum * count;

      entries.emplace_back(MwCASEntry{&target, old_val, new_val});
      const auto mwcas_result = manager.MwCAS(std::move(entries));

      const auto read_val = MwCASManager::ReadMwCASField<uint64_t>(&target);

      EXPECT_TRUE(mwcas_result);
      EXPECT_EQ(new_val, read_val);
    }
  };

  f(0);
}

TEST_F(MwCASManagerFixture, MwCAS_OneFieldTwoThreads_ReadValidValues)
{
  constexpr auto kLoopNum = 1000UL;
  constexpr auto kThreadNum = 2UL;

  const auto init = 9999UL;
  auto target = uint64_t{init};

  auto f = [&manager = manager, &target = target](const uint64_t begin_index) {
    for (uint64_t count = 0; count < kLoopNum; ++count) {
      std::vector<MwCASEntry> entries;

      const auto old_val = MwCASManager::ReadMwCASField<uint64_t>(&target);
      const auto new_val = begin_index + kThreadNum * count;

      entries.emplace_back(MwCASEntry{&target, old_val, new_val});
      const auto mwcas_success = manager.MwCAS(std::move(entries));

      const auto read_val = MwCASManager::ReadMwCASField<uint64_t>(&target);
      const auto expected = (mwcas_success) ? new_val : old_val;
      const auto result_is_valid = read_val == expected || read_val % kThreadNum != begin_index;

      EXPECT_TRUE(result_is_valid);
    }
  };

  auto t1 = std::thread{f, 0};
  auto t2 = std::thread{f, 1};
  t1.join();
  t2.join();
}

TEST_F(MwCASManagerFixture, MwCAS_TwoFieldsSingleThread_ReadValidValues)
{
  constexpr auto kLoopNum = 1000UL;
  constexpr auto kThreadNum = 1UL;

  const auto init = 9999UL;
  auto target_1 = uint64_t{init};
  auto target_2 = uint64_t{init};

  auto f = [&manager = manager, &target_1 = target_1,
            &target_2 = target_2](const uint64_t begin_index) {
    for (uint64_t count = 0; count < kLoopNum; ++count) {
      std::vector<MwCASEntry> entries;

      const auto old_1 = MwCASManager::ReadMwCASField<uint64_t>(&target_1);
      const auto new_1 = begin_index + kThreadNum * count;
      const auto old_2 = MwCASManager::ReadMwCASField<uint64_t>(&target_2);
      const auto new_2 = begin_index + kThreadNum * count;

      entries.emplace_back(MwCASEntry{&target_1, old_1, new_1});
      entries.emplace_back(MwCASEntry{&target_2, old_2, new_2});
      const auto mwcas_result = manager.MwCAS(std::move(entries));

      const auto read_1 = MwCASManager::ReadMwCASField<uint64_t>(&target_1);
      const auto read_2 = MwCASManager::ReadMwCASField<uint64_t>(&target_2);

      EXPECT_TRUE(mwcas_result);
      EXPECT_EQ(new_1, read_1);
      EXPECT_EQ(new_2, read_2);
    }
  };

  f(0);
}

}  // namespace dbgroup::atomic::mwcas
