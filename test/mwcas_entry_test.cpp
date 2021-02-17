// Copyright (c) DB Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#include "mwcas_entry.hpp"

#include "gtest/gtest.h"

namespace dbgroup::atomic::mwcas
{
class MwCASEntryFixture : public ::testing::Test
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

TEST_F(MwCASEntryFixture, Construct_InitialValues_MemberVariablesCorrectlyInitialized)
{
  auto target = uint32_t{10};
  const auto old_val = target;
  const auto new_val = uint32_t{20};

  const auto entry = MwCASEntry{&target, old_val, new_val};

  EXPECT_EQ(static_cast<void*>(&target), static_cast<void*>(entry.addr));
  EXPECT_EQ(old_val, entry.old_val.GetTargetData<uint32_t>());
  EXPECT_EQ(new_val, entry.new_val.GetTargetData<uint32_t>());
}

}  // namespace dbgroup::atomic::mwcas
