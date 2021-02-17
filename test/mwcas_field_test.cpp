// Copyright (c) DB Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#include "mwcas_field.hpp"

#include "cas_n_field.hpp"
#include "gtest/gtest.h"

namespace dbgroup::atomic::mwcas
{
class MwCASFieldFixture : public ::testing::Test
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

TEST_F(MwCASFieldFixture, Construct_DescriptorFlagOff_MemberVariablesCorrectlyInitialized)
{
  const auto data_1 = uint64_t{0};
  const auto target_word_1 = MwCASField(data_1);

  EXPECT_FALSE(target_word_1.IsMwCASDescriptor());
  EXPECT_EQ(data_1, target_word_1.GetTargetData<uint64_t>());

  const auto data_2 = int32_t{-1};
  const auto target_word_2 = MwCASField(data_2);

  EXPECT_FALSE(target_word_2.IsMwCASDescriptor());
  EXPECT_EQ(data_2, target_word_2.GetTargetData<int32_t>());
}

TEST_F(MwCASFieldFixture, Construct_DescriptorFlagOn_MemberVariablesCorrectlyInitialized)
{
  const auto data_1 = uint64_t{0};
  const auto target_word_1 = MwCASField(data_1, true);

  EXPECT_FALSE(target_word_1.IsRDCSSDescriptor());
  EXPECT_TRUE(target_word_1.IsMwCASDescriptor());
  EXPECT_EQ(data_1, target_word_1.GetTargetData<uint64_t>());

  const auto data_2 = int32_t{-1};
  const auto target_word_2 = MwCASField(data_2, true);

  EXPECT_FALSE(target_word_2.IsRDCSSDescriptor());
  EXPECT_TRUE(target_word_2.IsMwCASDescriptor());
  EXPECT_EQ(data_2, target_word_2.GetTargetData<int32_t>());

  const auto target_word_3 = MwCASField(data_1, false, true);

  EXPECT_TRUE(target_word_3.IsRDCSSDescriptor());
  EXPECT_FALSE(target_word_3.IsMwCASDescriptor());
  EXPECT_EQ(data_1, target_word_3.GetTargetData<uint64_t>());

  const auto target_word_4 = MwCASField(data_2, false, true);

  EXPECT_TRUE(target_word_4.IsRDCSSDescriptor());
  EXPECT_FALSE(target_word_4.IsMwCASDescriptor());
  EXPECT_EQ(data_2, target_word_4.GetTargetData<int32_t>());
}

TEST_F(MwCASFieldFixture, EqualOp_EqualInstances_ReturnTrue)
{
  auto data_1 = int32_t{10};
  auto data_2 = int32_t{10};
  auto target_word_1 = MwCASField(data_1);
  auto target_word_2 = MwCASField(data_2);

  EXPECT_TRUE(target_word_1 == target_word_2);

  target_word_1 = MwCASField(data_1, true);
  target_word_2 = MwCASField(data_2, true);

  EXPECT_TRUE(target_word_1 == target_word_2);

  target_word_1 = MwCASField(data_1, false, true);
  target_word_2 = MwCASField(data_2, false, true);

  EXPECT_TRUE(target_word_1 == target_word_2);
}

TEST_F(MwCASFieldFixture, EqualOp_DifferentInstances_ReturnFalse)
{
  auto data_1 = int32_t{10};
  auto data_2 = int32_t{1};
  auto target_word_1 = MwCASField(data_1);
  auto target_word_2 = MwCASField(data_2);

  EXPECT_FALSE(target_word_1 == target_word_2);

  data_1 = int32_t{10};
  data_2 = int32_t{10};
  target_word_1 = MwCASField(data_1, true);
  target_word_2 = MwCASField(data_2, false);

  EXPECT_FALSE(target_word_1 == target_word_2);

  target_word_1 = MwCASField(data_1, false, true);
  target_word_2 = MwCASField(data_2, false, false);

  EXPECT_FALSE(target_word_1 == target_word_2);
}

TEST_F(MwCASFieldFixture, NotEqualOp_EqualInstances_ReturnFalse)
{
  auto data_1 = int32_t{10};
  auto data_2 = int32_t{10};
  auto target_word_1 = MwCASField(data_1);
  auto target_word_2 = MwCASField(data_2);

  EXPECT_FALSE(target_word_1 != target_word_2);

  target_word_1 = MwCASField(data_1, true);
  target_word_2 = MwCASField(data_2, true);

  EXPECT_FALSE(target_word_1 != target_word_2);

  target_word_1 = MwCASField(data_1, false, true);
  target_word_2 = MwCASField(data_2, false, true);

  EXPECT_FALSE(target_word_1 != target_word_2);
}

TEST_F(MwCASFieldFixture, NotEqualOp_DifferentInstances_ReturnTrue)
{
  auto data_1 = int32_t{10};
  auto data_2 = int32_t{1};
  auto target_word_1 = MwCASField(data_1);
  auto target_word_2 = MwCASField(data_2);

  EXPECT_TRUE(target_word_1 != target_word_2);

  data_1 = int32_t{10};
  data_2 = int32_t{10};
  target_word_1 = MwCASField(data_1, true);
  target_word_2 = MwCASField(data_2, false);

  EXPECT_TRUE(target_word_1 != target_word_2);

  target_word_1 = MwCASField(data_1, false, true);
  target_word_2 = MwCASField(data_2, false, false);

  EXPECT_TRUE(target_word_1 != target_word_2);
}

}  // namespace dbgroup::atomic::mwcas
