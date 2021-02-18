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
  static constexpr auto kLoopNum = 100000UL;
  static constexpr auto kInitVal = 999999UL;

  MwCASManager manager{1};
  uint64_t target_1{kInitVal};
  uint64_t target_2{kInitVal};

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
  constexpr auto kThreadNum = 1UL;

  auto f = [&manager = manager, &target_1 = target_1](const uint64_t begin_index) {
    for (uint64_t count = 0; count < kLoopNum; ++count) {
      std::vector<MwCASEntry> entries;

      const auto old_val = MwCASManager::ReadMwCASField<uint64_t>(&target_1);
      const auto new_val = begin_index + kThreadNum * count;

      entries.emplace_back(MwCASEntry{&target_1, old_val, new_val});
      const auto mwcas_result = manager.MwCAS(std::move(entries));

      const auto read_val = MwCASManager::ReadMwCASField<uint64_t>(&target_1);

      EXPECT_TRUE(mwcas_result);
      EXPECT_EQ(new_val, read_val);
    }
  };

  f(0);
}

TEST_F(MwCASManagerFixture, MwCAS_OneFieldTwoThreads_ReadValidValues)
{
  constexpr auto kThreadNum = 2UL;

  auto f = [&manager = manager, &target_1 = target_1](const uint64_t begin_index) {
    for (uint64_t count = 0; count < kLoopNum; ++count) {
      std::vector<MwCASEntry> entries;

      const auto old_val = MwCASManager::ReadMwCASField<uint64_t>(&target_1);
      const auto new_val = begin_index + kThreadNum * count;

      entries.emplace_back(MwCASEntry{&target_1, old_val, new_val});
      const auto mwcas_success = manager.MwCAS(std::move(entries));

      const auto read_val = MwCASManager::ReadMwCASField<uint64_t>(&target_1);
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

TEST_F(MwCASManagerFixture, MwCAS_OneFieldTenThreads_ReadValidValues)
{
  constexpr auto kThreadNum = 10UL;

  auto f = [&manager = manager, &target_1 = target_1](const uint64_t begin_index) {
    for (uint64_t count = 0; count < kLoopNum; ++count) {
      std::vector<MwCASEntry> entries;

      const auto old_val = MwCASManager::ReadMwCASField<uint64_t>(&target_1);
      const auto new_val = begin_index + kThreadNum * count;

      entries.emplace_back(MwCASEntry{&target_1, old_val, new_val});
      const auto mwcas_success = manager.MwCAS(std::move(entries));

      const auto read_val = MwCASManager::ReadMwCASField<uint64_t>(&target_1);
      const auto expected = (mwcas_success) ? new_val : old_val;
      const auto result_is_valid = read_val == expected || read_val % kThreadNum != begin_index;

      EXPECT_TRUE(result_is_valid);
    }
  };

  std::vector<std::thread> threads;
  for (size_t count = 0; count < kThreadNum; ++count) {
    threads.emplace_back(std::thread{f, count});
  }
  for (auto &&thread : threads) {
    thread.join();
  }
}

TEST_F(MwCASManagerFixture, MwCAS_TwoFieldsSingleThread_ReadValidValues)
{
  constexpr auto kThreadNum = 1UL;

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

TEST_F(MwCASManagerFixture, MwCAS_TwoFieldsTwoThreads_ReadValidValues)
{
  constexpr auto kInnerLoopNum = 100;
  constexpr auto kOuterLoopNum = kLoopNum / kInnerLoopNum;
  constexpr auto kThreadNum = 2UL;

  auto f = [&manager = manager, &target_1 = target_1,
            &target_2 = target_2](const uint64_t begin_index) {
    for (uint64_t count = 0; count < kInnerLoopNum; ++count) {
      std::vector<MwCASEntry> entries;

      const auto old_1 = MwCASManager::ReadMwCASField<uint64_t>(&target_1);
      const auto new_1 = begin_index + kThreadNum * count;
      const auto old_2 = MwCASManager::ReadMwCASField<uint64_t>(&target_2);
      const auto new_2 = begin_index + kThreadNum * count;

      entries.emplace_back(MwCASEntry{&target_1, old_1, new_1});
      entries.emplace_back(MwCASEntry{&target_2, old_2, new_2});
      const auto mwcas_success = manager.MwCAS(std::move(entries));

      const auto read_1 = MwCASManager::ReadMwCASField<uint64_t>(&target_1);
      const auto read_2 = MwCASManager::ReadMwCASField<uint64_t>(&target_2);

      const auto expected_1 = (mwcas_success) ? new_1 : old_1;
      const auto result_1_is_valid = read_1 == expected_1 || read_1 % kThreadNum != begin_index;
      const auto expected_2 = (mwcas_success) ? new_2 : old_2;
      const auto result_2_is_valid = read_2 == expected_2 || read_2 % kThreadNum != begin_index;

      EXPECT_TRUE(result_1_is_valid);
      EXPECT_TRUE(result_2_is_valid);
    }
  };

  for (size_t count = 0; count < kOuterLoopNum; ++count) {
    auto t1 = std::thread{f, 0};
    auto t2 = std::thread{f, 1};
    t1.join();
    t2.join();

    const auto read_1 = MwCASManager::ReadMwCASField<uint64_t>(&target_1);
    const auto read_2 = MwCASManager::ReadMwCASField<uint64_t>(&target_2);

    EXPECT_EQ(read_1, read_2);
  }
}

}  // namespace dbgroup::atomic::mwcas
