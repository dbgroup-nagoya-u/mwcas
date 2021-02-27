// Copyright (c) Database Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <atomic>

#include "common.hpp"

namespace dbgroup::atomic::mwcas
{
/**
 * @brief A class to represent a MwCAS target field.
 *
 */
class alignas(kWordSize) MwCASField
{
 protected:
  /*################################################################################################
   * Internal member variables
   *##############################################################################################*/

  /// An actual target data
  uint64_t target_bit_arr_ : 63;

  /// Representing whether this field contains a MwCAS descriptor
  uint64_t mwcas_flag_ : 1;

 public:
  /*################################################################################################
   * Public constructors/destructors
   *##############################################################################################*/

  constexpr MwCASField() : target_bit_arr_{0}, mwcas_flag_{0} {}

  template <class T>
  constexpr MwCASField(  //
      const T target_data,
      const bool is_mwcas_descriptor = false)
      : target_bit_arr_{CASTargetConverter{target_data}.converted_data},
        mwcas_flag_{is_mwcas_descriptor}
  {
  }

  ~MwCASField() = default;

  MwCASField(const MwCASField &) = default;
  MwCASField &operator=(const MwCASField &obj) = default;
  MwCASField(MwCASField &&) = default;
  MwCASField &operator=(MwCASField &&) = default;

  constexpr bool
  operator==(const MwCASField &obj) const
  {
    return this->mwcas_flag_ == obj.mwcas_flag_ && this->target_bit_arr_ == obj.target_bit_arr_;
  }

  constexpr bool
  operator!=(const MwCASField &obj) const
  {
    return this->mwcas_flag_ != obj.mwcas_flag_ || this->target_bit_arr_ != obj.target_bit_arr_;
  }

  /*################################################################################################
   * Public getters/setters
   *##############################################################################################*/

  constexpr bool
  IsMwCASDescriptor() const
  {
    return mwcas_flag_;
  }

  template <class T>
  constexpr T
  GetTargetData() const
  {
    return CASTargetConverter<T>{target_bit_arr_}.target_data;
  }
};

// CAS target words must be one word
static_assert(sizeof(MwCASField) == kWordSize);

}  // namespace dbgroup::atomic::mwcas
