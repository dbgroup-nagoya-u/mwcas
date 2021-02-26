// Copyright (c) DB Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#include "mwcas/mwcas_manager.hpp"

#include <gtest/gtest.h>

#include <utility>
#include <vector>

namespace dbgroup::atomic::mwcas
{
class MwCASManagerFixture : public ::testing::Test
{
 public:
  static constexpr size_t kLoopNum = 1E4;
  static constexpr auto kInitVal = 999999UL;

  dbgroup::atomic::MwCASManager manager{};
  uint64_t target_1{kInitVal};
  uint64_t target_2{kInitVal};
  uint64_t target_3{kInitVal};
  uint64_t target_4{kInitVal};

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

  auto f = [&](const uint64_t begin_index) {
    for (uint64_t count = 0; count < kLoopNum; ++count) {
      auto desc = manager.CreateMwCASDescriptor();

      const auto old_val = manager.ReadMwCASField<uint64_t>(&target_1);
      const auto new_val = begin_index + kThreadNum * count;

      desc->AddEntry(&target_1, old_val, new_val);

      const auto mwcas_success = manager.MwCAS(desc);

      const auto read_val = manager.ReadMwCASField<uint64_t>(&target_1);

      EXPECT_TRUE(mwcas_success);
      EXPECT_EQ(new_val, read_val);

      std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
  };

  std::thread{f, 0}.join();
}

TEST_F(MwCASManagerFixture, MwCAS_OneFieldTwoThreads_ReadValidValues)
{
  constexpr auto kThreadNum = 2UL;

  auto f = [&](const uint64_t begin_index) {
    for (uint64_t count = 0; count < kLoopNum; ++count) {
      auto desc = manager.CreateMwCASDescriptor();

      const auto old_val = manager.ReadMwCASField<uint64_t>(&target_1);
      const auto new_val = begin_index + kThreadNum * count;

      desc->AddEntry(&target_1, old_val, new_val);

      const auto mwcas_success = manager.MwCAS(desc);

      const auto read_val = manager.ReadMwCASField<uint64_t>(&target_1);
      const auto expected = (mwcas_success) ? new_val : old_val;
      const auto result_is_valid = read_val == expected || read_val % kThreadNum != begin_index;

      EXPECT_TRUE(result_is_valid);

      std::this_thread::sleep_for(std::chrono::microseconds(50));
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

  auto f = [&](const uint64_t begin_index) {
    for (uint64_t count = 0; count < kLoopNum; ++count) {
      auto desc = manager.CreateMwCASDescriptor();

      const auto old_val = manager.ReadMwCASField<uint64_t>(&target_1);
      const auto new_val = begin_index + kThreadNum * count;

      desc->AddEntry(&target_1, old_val, new_val);

      const auto mwcas_success = manager.MwCAS(desc);

      const auto read_val = manager.ReadMwCASField<uint64_t>(&target_1);
      const auto expected = (mwcas_success) ? new_val : old_val;
      const auto result_is_valid = read_val == expected || read_val % kThreadNum != begin_index;

      EXPECT_TRUE(result_is_valid);

      std::this_thread::sleep_for(std::chrono::microseconds(50));
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

  auto f = [&](const uint64_t begin_index) {
    for (uint64_t count = 0; count < kLoopNum; ++count) {
      auto desc = manager.CreateMwCASDescriptor();

      const auto old_1 = manager.ReadMwCASField<uint64_t>(&target_1);
      const auto new_1 = begin_index + kThreadNum * count;
      const auto old_2 = manager.ReadMwCASField<uint64_t>(&target_2);
      const auto new_2 = begin_index + kThreadNum * count;

      desc->AddEntry(&target_1, old_1, new_1);
      desc->AddEntry(&target_2, old_2, new_2);

      const auto mwcas_success = manager.MwCAS(desc);

      const auto read_1 = manager.ReadMwCASField<uint64_t>(&target_1);
      const auto read_2 = manager.ReadMwCASField<uint64_t>(&target_2);

      EXPECT_TRUE(mwcas_success);
      EXPECT_EQ(new_1, read_1);
      EXPECT_EQ(new_2, read_2);

      std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
  };

  std::thread{f, 0}.join();
}

TEST_F(MwCASManagerFixture, MwCAS_TwoFieldsTwoThreads_ReadValidValues)
{
  constexpr size_t kInnerLoopNum = 1E3;
  constexpr auto kOuterLoopNum = kLoopNum / kInnerLoopNum;
  constexpr auto kThreadNum = 2UL;

  auto f = [&](const uint64_t begin_index) {
    for (uint64_t count = 0; count < kInnerLoopNum; ++count) {
      auto desc = manager.CreateMwCASDescriptor();

      const auto old_1 = manager.ReadMwCASField<uint64_t>(&target_1);
      const auto new_1 = begin_index + kThreadNum * count;
      const auto old_2 = manager.ReadMwCASField<uint64_t>(&target_2);
      const auto new_2 = begin_index + kThreadNum * count;

      desc->AddEntry(&target_1, old_1, new_1);
      desc->AddEntry(&target_2, old_2, new_2);

      const auto mwcas_success = manager.MwCAS(desc);

      const auto read_1 = manager.ReadMwCASField<uint64_t>(&target_1);
      const auto read_2 = manager.ReadMwCASField<uint64_t>(&target_2);

      const auto expected_1 = (mwcas_success) ? new_1 : old_1;
      const auto result_1_is_valid = read_1 == expected_1 || read_1 % kThreadNum != begin_index;
      const auto expected_2 = (mwcas_success) ? new_2 : old_2;
      const auto result_2_is_valid = read_2 == expected_2 || read_2 % kThreadNum != begin_index;

      EXPECT_TRUE(result_1_is_valid);
      EXPECT_TRUE(result_2_is_valid);

      std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
  };

  for (size_t count = 0; count < kOuterLoopNum; ++count) {
    auto t1 = std::thread{f, 0};
    auto t2 = std::thread{f, 1};
    t1.join();
    t2.join();

    const auto read_1 = manager.ReadMwCASField<uint64_t>(&target_1);
    const auto read_2 = manager.ReadMwCASField<uint64_t>(&target_2);

    EXPECT_EQ(read_1, read_2);
  }
}

TEST_F(MwCASManagerFixture, MwCAS_TwoFieldsTenThreads_ReadValidValues)
{
  constexpr size_t kInnerLoopNum = 1E3;
  constexpr auto kOuterLoopNum = kLoopNum / kInnerLoopNum;
  constexpr auto kThreadNum = 10UL;

  auto f = [&](const uint64_t begin_index) {
    for (uint64_t count = 0; count < kInnerLoopNum; ++count) {
      auto desc = manager.CreateMwCASDescriptor();

      const auto old_1 = manager.ReadMwCASField<uint64_t>(&target_1);
      const auto new_1 = begin_index + kThreadNum * count;
      const auto old_2 = manager.ReadMwCASField<uint64_t>(&target_2);
      const auto new_2 = begin_index + kThreadNum * count;

      desc->AddEntry(&target_1, old_1, new_1);
      desc->AddEntry(&target_2, old_2, new_2);

      const auto mwcas_success = manager.MwCAS(desc);

      const auto read_1 = manager.ReadMwCASField<uint64_t>(&target_1);
      const auto read_2 = manager.ReadMwCASField<uint64_t>(&target_2);

      const auto expected_1 = (mwcas_success) ? new_1 : old_1;
      const auto result_1_is_valid = read_1 == expected_1 || read_1 % kThreadNum != begin_index;
      const auto expected_2 = (mwcas_success) ? new_2 : old_2;
      const auto result_2_is_valid = read_2 == expected_2 || read_2 % kThreadNum != begin_index;

      EXPECT_TRUE(result_1_is_valid);
      EXPECT_TRUE(result_2_is_valid);

      std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
  };

  for (size_t count = 0; count < kOuterLoopNum; ++count) {
    std::vector<std::thread> threads;
    for (size_t index = 0; index < kThreadNum; ++index) {
      threads.emplace_back(std::thread{f, index});
    }
    for (auto &&thread : threads) {
      thread.join();
    }

    const auto read_1 = manager.ReadMwCASField<uint64_t>(&target_1);
    const auto read_2 = manager.ReadMwCASField<uint64_t>(&target_2);

    EXPECT_EQ(read_1, read_2);
  }
}

TEST_F(MwCASManagerFixture, MwCAS_FourFieldsTenThreads_ReadValidValues)
{
  constexpr size_t kInnerLoopNum = 1E3;
  constexpr auto kOuterLoopNum = kLoopNum / kInnerLoopNum;
  constexpr auto kThreadNum = 10UL;

  auto f = [&](const uint64_t begin_index) {
    for (uint64_t count = 0; count < kInnerLoopNum; ++count) {
      auto desc = manager.CreateMwCASDescriptor();

      const auto old_1 = manager.ReadMwCASField<uint64_t>(&target_1);
      const auto new_1 = begin_index + kThreadNum * count;
      const auto old_2 = manager.ReadMwCASField<uint64_t>(&target_2);
      const auto new_2 = begin_index + kThreadNum * count;
      const auto old_3 = manager.ReadMwCASField<uint64_t>(&target_3);
      const auto new_3 = begin_index + kThreadNum * count;
      const auto old_4 = manager.ReadMwCASField<uint64_t>(&target_4);
      const auto new_4 = begin_index + kThreadNum * count;

      desc->AddEntry(&target_1, old_1, new_1);
      desc->AddEntry(&target_2, old_2, new_2);
      desc->AddEntry(&target_3, old_3, new_3);
      desc->AddEntry(&target_4, old_4, new_4);

      const auto mwcas_success = manager.MwCAS(desc);

      const auto read_1 = manager.ReadMwCASField<uint64_t>(&target_1);
      const auto read_2 = manager.ReadMwCASField<uint64_t>(&target_2);
      const auto read_3 = manager.ReadMwCASField<uint64_t>(&target_3);
      const auto read_4 = manager.ReadMwCASField<uint64_t>(&target_4);

      const auto expected_1 = (mwcas_success) ? new_1 : old_1;
      const auto result_1_is_valid = read_1 == expected_1 || read_1 % kThreadNum != begin_index;
      const auto expected_2 = (mwcas_success) ? new_2 : old_2;
      const auto result_2_is_valid = read_2 == expected_2 || read_2 % kThreadNum != begin_index;
      const auto expected_3 = (mwcas_success) ? new_3 : old_3;
      const auto result_3_is_valid = read_3 == expected_3 || read_3 % kThreadNum != begin_index;
      const auto expected_4 = (mwcas_success) ? new_2 : old_2;
      const auto result_4_is_valid = read_4 == expected_4 || read_4 % kThreadNum != begin_index;

      EXPECT_TRUE(result_1_is_valid);
      EXPECT_TRUE(result_2_is_valid);
      EXPECT_TRUE(result_3_is_valid);
      EXPECT_TRUE(result_4_is_valid);

      std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
  };

  for (size_t count = 0; count < kOuterLoopNum; ++count) {
    std::vector<std::thread> threads;
    for (size_t index = 0; index < kThreadNum; ++index) {
      threads.emplace_back(std::thread{f, index});
    }
    for (auto &&thread : threads) {
      thread.join();
    }

    const auto read_1 = manager.ReadMwCASField<uint64_t>(&target_1);
    const auto read_2 = manager.ReadMwCASField<uint64_t>(&target_2);
    const auto read_3 = manager.ReadMwCASField<uint64_t>(&target_3);
    const auto read_4 = manager.ReadMwCASField<uint64_t>(&target_4);

    EXPECT_EQ(read_1, read_2);
    EXPECT_EQ(read_1, read_3);
    EXPECT_EQ(read_1, read_4);
  }
}

}  // namespace dbgroup::atomic::mwcas
