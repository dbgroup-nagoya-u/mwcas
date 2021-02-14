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
class alignas(kWordSize) RDCSSTargetWord
{
 private:
  /*################################################################################################
   * Internal member variables
   *##############################################################################################*/

  uint64_t rdcss_flag_ : 1;

  uint64_t target_bit_arr_ : 63;

 public:
  /*################################################################################################
   * Public constructors/destructors
   *##############################################################################################*/

  constexpr RDCSSTargetWord() : rdcss_flag_{0}, target_bit_arr_{0} {}

  template <class T>
  constexpr RDCSSTargetWord(  //
      const T target_data,
      const bool is_rdcss_descriptor = false)
      : rdcss_flag_{is_rdcss_descriptor},
        target_bit_arr_{CASTargetConverter{target_data}.converted_data}
  {
  }

  ~RDCSSTargetWord() {}

  RDCSSTargetWord(const RDCSSTargetWord &) = default;
  RDCSSTargetWord &operator=(const RDCSSTargetWord &obj) = default;
  RDCSSTargetWord(RDCSSTargetWord &&) = default;
  RDCSSTargetWord &operator=(RDCSSTargetWord &&) = default;

  constexpr bool
  operator==(const RDCSSTargetWord &obj) const
  {
    return this->rdcss_flag_ == obj.rdcss_flag_ && this->target_bit_arr_ == obj.target_bit_arr_;
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

  constexpr uint64_t
  GetTargetAsUInt64() const
  {
    return target_bit_arr_;
  }
};

// CAS target words must be one word
static_assert(sizeof(RDCSSTargetWord) == kWordSize);

}  // namespace dbgroup::atomic::mwcas
