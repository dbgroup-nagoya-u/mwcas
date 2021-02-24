// Copyright (c) DB Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#include "mwcas/casn/rdcss_descriptor.hpp"

#include <gtest/gtest.h>

#include <thread>
#include <vector>

#include "gc/tls_based_gc.hpp"

namespace dbgroup::atomic::mwcas
{
using ::dbgroup::gc::TLSBasedGC;

class RDCSSDescriptorFixture : public ::testing::Test
{
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

TEST_F(RDCSSDescriptorFixture, Construct_SetTargets_ReadInitialValue)
{
  const auto old_1 = size_t{1};
  const auto old_2 = size_t{2};
  const auto new_2 = size_t{20};

  auto target_1 = RDCSSField{old_1};
  auto target_2 = RDCSSField{old_2};

  auto desc = RDCSSDescriptor{&target_1, old_1, &target_2, old_2, new_2};

  const auto read_2 = RDCSSDescriptor::ReadRDCSSField<size_t>(&target_2);

  EXPECT_EQ(old_2, read_2);
}

TEST_F(RDCSSDescriptorFixture, ReadRDCSSField_AfterCAS_ReadNewValue)
{
  const auto old_1 = size_t{1};
  const auto old_2 = size_t{2};
  const auto new_2 = size_t{20};

  auto target_1 = RDCSSField{old_1};
  auto target_2 = RDCSSField{old_2};

  auto desc = RDCSSDescriptor{&target_1, old_1, &target_2, old_2, new_2};

  desc.RDCSS();
  const auto read_2 = RDCSSDescriptor::ReadRDCSSField<size_t>(&target_2);

  EXPECT_EQ(new_2, read_2);
}

TEST_F(RDCSSDescriptorFixture, RDCSS_TwoThreads_ReturnValidValues)
{
  auto gc = TLSBasedGC<RDCSSDescriptor>{100};

  constexpr auto kLoopNum = 1000UL;

  const auto init_1 = size_t{0};
  const auto init_2 = size_t{1000};

  auto target_1 = RDCSSField{init_1};
  auto target_2 = RDCSSField{init_2};

  auto f = [addr_1 = &target_1, addr_2 = &target_2, init_1, &gc = gc](const size_t begin_index) {
    for (size_t i = 0; i < kLoopNum; i++) {
      const auto guard = gc.CreateEpochGuard();

      const auto old_2 = RDCSSDescriptor::ReadRDCSSField<size_t>(addr_2);
      const auto new_2 = begin_index + 2 * i;
      auto desc = new RDCSSDescriptor{addr_1, init_1, addr_2, old_2, new_2};
      const auto return_field = desc->RDCSS();

      const auto result =
          !return_field.IsRDCSSDescriptor()
          && (return_field == old_2 || return_field.GetTargetData<size_t>() % 2 != begin_index);

      EXPECT_TRUE(result);

      gc.AddGarbage(desc);

      std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
  };

  auto t1 = std::thread{f, 0};
  auto t2 = std::thread{f, 1};
  t1.join();
  t2.join();
}

TEST_F(RDCSSDescriptorFixture, RDCSS_TenThreads_ReturnValidValues)
{
  auto gc = TLSBasedGC<RDCSSDescriptor>{100};

  constexpr auto kLoopNum = 1000UL;
  constexpr auto kThreadNum = 10UL;

  const auto init_1 = size_t{0};
  const auto init_2 = size_t{1000};

  auto target_1 = RDCSSField{init_1};
  auto target_2 = RDCSSField{init_2};

  auto f = [addr_1 = &target_1, addr_2 = &target_2, init_1, &gc = gc](const size_t begin_index) {
    for (size_t i = 0; i < kLoopNum; i++) {
      const auto guard = gc.CreateEpochGuard();

      const auto old_2 = RDCSSDescriptor::ReadRDCSSField<size_t>(addr_2);
      const auto new_2 = begin_index + kThreadNum * i;
      auto desc = new RDCSSDescriptor{addr_1, init_1, addr_2, old_2, new_2};
      const auto return_field = desc->RDCSS();

      const auto result_val = return_field.GetTargetData<size_t>();
      const auto result = !return_field.IsRDCSSDescriptor()
                          && (result_val == old_2 || result_val % kThreadNum != begin_index);

      EXPECT_TRUE(result);

      gc.AddGarbage(desc);

      std::this_thread::sleep_for(std::chrono::microseconds(50));
    }
  };

  std::vector<std::thread> threads;
  for (size_t i = 0; i < kThreadNum; ++i) {
    threads.emplace_back(std::thread{f, i});
  }
  for (auto &&thread : threads) {
    thread.join();
  }
}

}  // namespace dbgroup::atomic::mwcas
