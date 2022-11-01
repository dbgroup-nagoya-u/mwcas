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

#include "mwcas/component/mwcas_target.hpp"

#include "common.hpp"
#include "gtest/gtest.h"

namespace dbgroup::atomic::mwcas::component::test
{
template <class Target>
class MwCASTargetFixture : public ::testing::Test
{
 protected:
  /*####################################################################################
   * Setup/Teardown
   *##################################################################################*/

  void
  SetUp() override
  {
    if constexpr (std::is_same_v<Target, uint64_t *>) {
      old_val_ = new uint64_t{1};
      new_val_ = new uint64_t{2};
    } else {
      old_val_ = 1;
      new_val_ = 2;
    }
    target_ = old_val_;

    mwcas_target_ = MwCASTarget{&target_, old_val_, new_val_, std::memory_order_relaxed};
    desc_ = MwCASField{0UL, true};
  }

  void
  TearDown() override
  {
    if constexpr (std::is_same_v<Target, uint64_t *>) {
      delete old_val_;
      delete new_val_;
    }
  }

  /*####################################################################################
   * Functions for verification
   *##################################################################################*/

  void
  VerifyEmbedDescriptor(const bool expect_fail)
  {
    if (expect_fail) {
      target_ = new_val_;
    }

    const bool result = mwcas_target_.EmbedDescriptor(desc_);

    if (expect_fail) {
      EXPECT_FALSE(result);
      EXPECT_NE(CASTargetConverter<MwCASField>{desc_}.converted_data,  // NOLINT
                CASTargetConverter<Target>{target_}.converted_data);   // NOLINT
    } else {
      EXPECT_TRUE(result);
      EXPECT_EQ(CASTargetConverter<MwCASField>{desc_}.converted_data,  // NOLINT
                CASTargetConverter<Target>{target_}.converted_data);   // NOLINT
    }
  }

  void
  VerifyCompleteMwCAS(const bool mwcas_success)
  {
    ASSERT_TRUE(mwcas_target_.EmbedDescriptor(desc_));

    if (mwcas_success) {
      mwcas_target_.RedoMwCAS();
      EXPECT_EQ(new_val_, target_);
    } else {
      mwcas_target_.UndoMwCAS();
      EXPECT_EQ(old_val_, target_);
    }
  }

 private:
  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

  MwCASTarget mwcas_target_;
  MwCASField desc_;

  Target target_{};
  Target old_val_{};
  Target new_val_{};
};

/*######################################################################################
 * Preparation for typed testing
 *####################################################################################*/

using Targets = ::testing::Types<uint64_t, uint64_t *, MyClass>;
TYPED_TEST_SUITE(MwCASTargetFixture, Targets);

/*######################################################################################
 * Unit test definitions
 *####################################################################################*/

TYPED_TEST(MwCASTargetFixture, EmbedDescriptorWithExpectedValueSucceedEmbedding)
{
  TestFixture::VerifyEmbedDescriptor(false);
}

TYPED_TEST(MwCASTargetFixture, EmbedDescriptorWithUnexpectedValueFailEmbedding)
{
  TestFixture::VerifyEmbedDescriptor(true);
}

TYPED_TEST(MwCASTargetFixture, CompleteMwCASWithSucceededStatusUpdateToDesiredValue)
{
  TestFixture::VerifyCompleteMwCAS(true);
}

TYPED_TEST(MwCASTargetFixture, CompleteMwCASWithFailedStatusRevertToExpectedValue)
{
  TestFixture::VerifyCompleteMwCAS(false);
}

}  // namespace dbgroup::atomic::mwcas::component::test
