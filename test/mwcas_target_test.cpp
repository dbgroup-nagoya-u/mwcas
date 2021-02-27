// Copyright (c) DB Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#include "mwcas/components/mwcas_target.hpp"

#include <gtest/gtest.h>

namespace dbgroup::atomic::mwcas
{
using Target = uint64_t;

class MwCASTargetFixture : public ::testing::Test
{
 public:
  static constexpr Target old_val = 1;
  static constexpr Target new_val = 2;

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

TEST_F(MwCASTargetFixture, Construct_InitialValues_MemberVariablesCorrectlyInitialized)
{
  auto target = old_val;

  const auto entry = MwCASTarget{&target, old_val, new_val};

  EXPECT_EQ(static_cast<void*>(&target), static_cast<void*>(entry.addr));
  EXPECT_EQ(old_val, entry.old_val.GetTargetData<Target>());
  EXPECT_EQ(new_val, entry.new_val.GetTargetData<Target>());
}

}  // namespace dbgroup::atomic::mwcas
