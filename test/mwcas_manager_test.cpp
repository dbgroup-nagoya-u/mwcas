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

}  // namespace dbgroup::atomic::mwcas
