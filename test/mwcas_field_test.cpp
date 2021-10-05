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

#include "common.hpp"

namespace dbgroup::atomic::mwcas::component::test
{
template <class Target>
class MwCASFieldFixture : public ::testing::Test
{
 protected:
  /*################################################################################################
   * Setup/Teardown
   *##############################################################################################*/

  void
  SetUp() override
  {
    if constexpr (std::is_same_v<Target, uint64_t*>) {
      data_1 = new uint64_t{1};
      data_2 = new uint64_t{2};
    } else {
      data_1 = 1;
      data_2 = 2;
    }
  }

  void
  TearDown() override
  {
    if constexpr (std::is_same_v<Target, uint64_t*>) {
      delete data_1;
      delete data_2;
    }
  }

  /*################################################################################################
   * Functions for verification
   *##############################################################################################*/

  void
  VerifyConstructor(const bool is_mwcas_desc)
  {
    const auto target_word_1 = MwCASField{data_1, is_mwcas_desc};

    if (is_mwcas_desc) {
      EXPECT_TRUE(target_word_1.IsMwCASDescriptor());
    } else {
      EXPECT_FALSE(target_word_1.IsMwCASDescriptor());
    }
    EXPECT_EQ(data_1, target_word_1.GetTargetData<Target>());
  }

  void
  VerifyEQ()
  {
    MwCASField field_a{data_1, false};
    MwCASField field_b{data_1, false};
    EXPECT_TRUE(field_a == field_b);

    field_b = MwCASField{data_2, false};
    EXPECT_FALSE(field_a == field_b);

    field_a = MwCASField{data_2, true};
    EXPECT_FALSE(field_a == field_b);

    field_b = MwCASField{data_2, true};
    EXPECT_TRUE(field_a == field_b);
  }

  void
  VerifyNE()
  {
    MwCASField field_a{data_1, false};
    MwCASField field_b{data_1, false};
    EXPECT_FALSE(field_a != field_b);

    field_b = MwCASField{data_2, false};
    EXPECT_TRUE(field_a != field_b);

    field_a = MwCASField{data_2, true};
    EXPECT_TRUE(field_a != field_b);

    field_b = MwCASField{data_2, true};
    EXPECT_FALSE(field_a != field_b);
  }

 private:
  /*################################################################################################
   * Internal member variables
   *##############################################################################################*/

  Target data_1;
  Target data_2;
};

/*##################################################################################################
 * Preparation for typed testing
 *################################################################################################*/

using Targets = ::testing::Types<uint64_t, uint64_t*, MyClass>;
TYPED_TEST_CASE(MwCASFieldFixture, Targets);

/*##################################################################################################
 * Unit test definitions
 *################################################################################################*/

TYPED_TEST(MwCASFieldFixture, Construct_DescriptorFlagOff_MemberVariablesCorrectlyInitialized)
{
  TestFixture::VerifyConstructor(false);
}

TYPED_TEST(MwCASFieldFixture, Construct_DescriptorFlagOn_MemberVariablesCorrectlyInitialized)
{
  TestFixture::VerifyConstructor(true);
}

TYPED_TEST(MwCASFieldFixture, EQ_AllCombinationOfInstances_ReturnCorrectBool)
{
  TestFixture::VerifyEQ();
}

TYPED_TEST(MwCASFieldFixture, NE_AllCombinationOfInstances_ReturnCorrectBool)
{
  TestFixture::VerifyNE();
}

}  // namespace dbgroup::atomic::mwcas::component::test
