// Copyright (c) Database Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <atomic>

#include "common.hpp"

namespace dbgroup::atomic::mwcas
{
/**
 * @brief A class to represent an RDCSS target word.
 *
 */
class alignas(kWordSize) RDCSSField
{
 private:
  /*################################################################################################
   * Internal member variables
   *##############################################################################################*/

  uint64_t target_bit_arr_ : 63;

  uint64_t rdcss_flag_ : 1;

 public:
  /*################################################################################################
   * Public constructors/destructors
   *##############################################################################################*/

  constexpr RDCSSField() : target_bit_arr_{0}, rdcss_flag_{0} {}

  template <class T>
  constexpr RDCSSField(  //
      const T target_data,
      const bool is_rdcss_descriptor = false)
      : target_bit_arr_{CASTargetConverter{target_data}.converted_data},
        rdcss_flag_{is_rdcss_descriptor}
  {
  }

  ~RDCSSField() = default;

  RDCSSField(const RDCSSField &) = default;
  RDCSSField &operator=(const RDCSSField &obj) = default;
  RDCSSField(RDCSSField &&) = default;
  RDCSSField &operator=(RDCSSField &&) = default;

  constexpr bool
  operator==(const RDCSSField &obj) const
  {
    return this->rdcss_flag_ == obj.rdcss_flag_ && this->target_bit_arr_ == obj.target_bit_arr_;
  }

  constexpr bool
  operator!=(const RDCSSField &obj) const
  {
    return this->rdcss_flag_ != obj.rdcss_flag_ || this->target_bit_arr_ != obj.target_bit_arr_;
  }

  /*################################################################################################
   * Public getters/setters
   *##############################################################################################*/

  constexpr bool
  IsRDCSSDescriptor() const
  {
    return rdcss_flag_ > 0;
  }

  template <class T>
  constexpr T
  GetTargetData() const
  {
    return CASTargetConverter<T>{target_bit_arr_}.target_data;
  }
};

// CAS target words must be one word
static_assert(sizeof(RDCSSField) == kWordSize);

}  // namespace dbgroup::atomic::mwcas
