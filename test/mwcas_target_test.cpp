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

#include "common.hpp"

namespace dbgroup::atomic::mwcas::component::test
{
template <class Target>

class MwCASTargetFixture : public ::testing::Test
{
 protected:
  /*################################################################################################
   * Setup/Teardown
   *##############################################################################################*/

  void
  SetUp() override
  {
    if constexpr (std::is_same_v<Target, uint64_t*>) {
      old_val = new uint64_t{1};
      new_val = new uint64_t{2};
    } else {
      old_val = 1;
      new_val = 2;
    }
    target = old_val;

    mwcas_target = MwCASTarget{&target, old_val, new_val};
    desc = MwCASField{0UL, true};
  }

  void
  TearDown() override
  {
    if constexpr (std::is_same_v<Target, uint64_t*>) {
      delete old_val;
      delete new_val;
    }
  }

  /*################################################################################################
   * Functions for verification
   *##############################################################################################*/

  void
  VerifyEmbedDescriptor(const bool expect_fail)
  {
    if (expect_fail) {
      target = new_val;
    }

    const bool result = mwcas_target.EmbedDescriptor(desc);

    if (expect_fail) {
      EXPECT_FALSE(result);
      EXPECT_NE(CASTargetConverter{desc}.converted_data, CASTargetConverter{target}.converted_data);
    } else {
      EXPECT_TRUE(result);
      EXPECT_EQ(CASTargetConverter{desc}.converted_data, CASTargetConverter{target}.converted_data);
    }
  }

  void
  VerifyCompleteMwCAS(const bool mwcas_success)
  {
    ASSERT_TRUE(mwcas_target.EmbedDescriptor(desc));

    mwcas_target.CompleteMwCAS(desc, mwcas_success);

    if (mwcas_success) {
      EXPECT_EQ(new_val, target);
    } else {
      EXPECT_EQ(old_val, target);
    }
  }

 private:
  /*################################################################################################
   * Internal member variables
   *##############################################################################################*/

  MwCASTarget mwcas_target;
  MwCASField desc;

  Target target;
  Target old_val;
  Target new_val;
};

/*##################################################################################################
 * Preparation for typed testing
 *################################################################################################*/

using Targets = ::testing::Types<uint64_t, uint64_t*, MyClass>;
TYPED_TEST_CASE(MwCASTargetFixture, Targets);

/*##################################################################################################
 * Unit test definitions
 *################################################################################################*/

TYPED_TEST(MwCASTargetFixture, EmbedDescriptor_TargetHasExpectedValue_EmbeddingSucceed)
{
  TestFixture::VerifyEmbedDescriptor(false);
}

TYPED_TEST(MwCASTargetFixture, EmbedDescriptor_TargetHasUnexpectedValue_EmbeddingFail)
{
  TestFixture::VerifyEmbedDescriptor(true);
}

TYPED_TEST(MwCASTargetFixture, CompleteMwCAS_MwCASSucceeded_UpdateToDesiredValue)
{
  TestFixture::VerifyCompleteMwCAS(true);
}

TYPED_TEST(MwCASTargetFixture, CompleteMwCAS_MwCASFailed_RevertToExpectedValue)
{
  TestFixture::VerifyCompleteMwCAS(false);
}

}  // namespace dbgroup::atomic::mwcas::component::test
