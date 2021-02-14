// Copyright (c) DB Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#include "rdcss_descriptor.hpp"

#include "gtest/gtest.h"
#include "rdcss_target_word.hpp"

namespace dbgroup::atomic::mwcas
{
class RDCSSDescriptorFixture : public ::testing::Test
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

TEST_F(RDCSSDescriptorFixture, ReadRDCSSField_ValuesSet_ReadSetValue)
{
  const auto old_1 = size_t{1};
  const auto old_2 = size_t{2};
  const auto new_2 = size_t{20};

  auto target_1 = RDCSSField{old_1};
  auto target_2 = RDCSSField{old_2};

  auto desc = RDCSSDescriptor{};
  desc.SetFirstTarget(&target_1, old_1);
  desc.SetSecondTarget(&target_2, old_2, new_2);

  const auto read_2 = RDCSSDescriptor::ReadRDCSSField<size_t>(&target_2);

  EXPECT_EQ(old_2, read_2);
}

}  // namespace dbgroup::atomic::mwcas
