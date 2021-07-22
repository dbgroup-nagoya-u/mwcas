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

#include "mwcas/components/mwcas_field.hpp"

#include <gtest/gtest.h>

namespace dbgroup::atomic::mwcas::component::test
{
using Target = uint64_t;

class CASNFieldFixture : public ::testing::Test
{
 public:
  static constexpr Target data_1 = 1;
  static constexpr Target data_2 = 2;

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

TEST_F(CASNFieldFixture, Construct_DescriptorFlagOff_MemberVariablesCorrectlyInitialized)
{
  const auto target_word_1 = MwCASField{data_1, false};

  EXPECT_FALSE(target_word_1.IsMwCASDescriptor());
  EXPECT_EQ(data_1, target_word_1.GetTargetData<Target>());

  const auto target_word_2 = MwCASField{data_2, false};

  EXPECT_FALSE(target_word_2.IsMwCASDescriptor());
  EXPECT_EQ(data_2, target_word_2.GetTargetData<Target>());
}

TEST_F(CASNFieldFixture, Construct_DescriptorFlagOn_MemberVariablesCorrectlyInitialized)
{
  const auto target_word_1 = MwCASField{data_1, true};

  EXPECT_TRUE(target_word_1.IsMwCASDescriptor());
  EXPECT_EQ(data_1, target_word_1.GetTargetData<Target>());

  const auto target_word_2 = MwCASField{data_2, true};

  EXPECT_TRUE(target_word_2.IsMwCASDescriptor());
  EXPECT_EQ(data_2, target_word_2.GetTargetData<Target>());
}

TEST_F(CASNFieldFixture, EqualOp_EqualInstances_ReturnTrue)
{
  auto target_word_1 = MwCASField{data_1, false};
  auto target_word_2 = MwCASField{data_1, false};

  EXPECT_TRUE(target_word_1 == target_word_2);

  target_word_1 = MwCASField{data_1, true};
  target_word_2 = MwCASField{data_1, true};

  EXPECT_TRUE(target_word_1 == target_word_2);
}

TEST_F(CASNFieldFixture, EqualOp_DifferentInstances_ReturnFalse)
{
  auto target_word_1 = MwCASField{data_1, false};
  auto target_word_2 = MwCASField{data_2, false};

  EXPECT_FALSE(target_word_1 == target_word_2);

  target_word_1 = MwCASField{data_1, true};
  target_word_2 = MwCASField{data_1, false};

  EXPECT_FALSE(target_word_1 == target_word_2);
}

TEST_F(CASNFieldFixture, NotEqualOp_EqualInstances_ReturnFalse)
{
  auto target_word_1 = MwCASField{data_1, false};
  auto target_word_2 = MwCASField{data_1, false};

  EXPECT_FALSE(target_word_1 != target_word_2);

  target_word_1 = MwCASField{data_1, true};
  target_word_2 = MwCASField{data_1, true};

  EXPECT_FALSE(target_word_1 != target_word_2);
}

TEST_F(CASNFieldFixture, NotEqualOp_DifferentInstances_ReturnTrue)
{
  auto target_word_1 = MwCASField{data_1, false};
  auto target_word_2 = MwCASField{data_2, false};

  EXPECT_TRUE(target_word_1 != target_word_2);

  target_word_1 = MwCASField{data_1, true};
  target_word_2 = MwCASField{data_1, false};

  EXPECT_TRUE(target_word_1 != target_word_2);
}

}  // namespace dbgroup::atomic::mwcas::component::test
