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

  const uint64_t rdcss_flag_ : 1;

  const uintptr_t target_bit_arr_ : 63;

 public:
  /*################################################################################################
   * Public constructors/destructors
   *##############################################################################################*/

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
  RDCSSTargetWord &operator=(const RDCSSTargetWord &) = default;
  RDCSSTargetWord(RDCSSTargetWord &&) = default;
  RDCSSTargetWord &operator=(RDCSSTargetWord &&) = default;

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

  constexpr uintptr_t
  GetDescriptorAddr() const
  {
    return target_bit_arr_;
  }
};

// CAS target words must be one word
static_assert(sizeof(RDCSSTargetWord) == kWordSize);

/**
 * @brief An union to convert a RDCSS target word into a std::atomic target.
 *
 */
union alignas(kWordSize) RDCSSField {
  RDCSSTargetWord word;
  std::atomic_uint64_t cas_target;
};

// CAS target filed must be represented by one word
static_assert(sizeof(RDCSSField) == kWordSize);

}  // namespace dbgroup::atomic::mwcas
