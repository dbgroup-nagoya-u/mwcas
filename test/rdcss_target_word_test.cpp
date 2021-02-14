// Copyright (c) DB Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#include "rdcss_target_word.hpp"

#include "gtest/gtest.h"

namespace dbgroup::atomic::mwcas
{
class RDCSSFieldFixture : public ::testing::Test
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

TEST_F(RDCSSFieldFixture, Construct_DescriptorFlagOff_MemberVariablesCorrectlyInitialized)
{
  const auto data_1 = uint64_t{0};
  const auto target_word_1 = RDCSSField(data_1);

  EXPECT_FALSE(target_word_1.IsRDCSSDescriptor());
  EXPECT_EQ(data_1, target_word_1.GetTargetData<uint64_t>());

  const auto data_2 = int32_t{-1};
  const auto target_word_2 = RDCSSField(data_2);

  EXPECT_FALSE(target_word_2.IsRDCSSDescriptor());
  EXPECT_EQ(data_2, target_word_2.GetTargetData<int32_t>());
}

TEST_F(RDCSSFieldFixture, Construct_DescriptorFlagOn_MemberVariablesCorrectlyInitialized)
{
  const auto data_1 = uint64_t{0};
  const auto target_word_1 = RDCSSField(data_1, true);

  EXPECT_TRUE(target_word_1.IsRDCSSDescriptor());
  EXPECT_EQ(data_1, target_word_1.GetTargetData<uint64_t>());

  const auto data_2 = int32_t{-1};
  const auto target_word_2 = RDCSSField(data_2, true);

  EXPECT_TRUE(target_word_2.IsRDCSSDescriptor());
  EXPECT_EQ(data_2, target_word_2.GetTargetData<int32_t>());
}

}  // namespace dbgroup::atomic::mwcas
