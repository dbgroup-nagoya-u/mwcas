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

#include "mwcas/components/mwcas_target.hpp"

#include <gtest/gtest.h>

namespace dbgroup::atomic::mwcas::component::test
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

}  // namespace dbgroup::atomic::mwcas::component::test
