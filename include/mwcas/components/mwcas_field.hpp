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

#pragma once

#include <atomic>

#include "common.hpp"

namespace dbgroup::atomic::mwcas::component
{
/**
 * @brief A class to represent a MwCAS target field.
 *
 */
class MwCASField
{
 public:
  /*################################################################################################
   * Public constructors and assignment operators
   *##############################################################################################*/

  constexpr MwCASField() : target_bit_arr_{}, mwcas_flag_{} {}

  constexpr MwCASField(  //
      const uint64_t target_data,
      const bool is_mwcas_descriptor = false)
      : target_bit_arr_{target_data}, mwcas_flag_{is_mwcas_descriptor}
  {
  }

  template <class T>
  constexpr MwCASField(  //
      const T target_data,
      const bool is_mwcas_descriptor = false)
      : target_bit_arr_{CASTargetConverter{target_data}.converted_data},
        mwcas_flag_{is_mwcas_descriptor}
  {
  }

  constexpr MwCASField(const MwCASField &) = default;
  constexpr MwCASField &operator=(const MwCASField &obj) = default;
  constexpr MwCASField(MwCASField &&) = default;
  constexpr MwCASField &operator=(MwCASField &&) = default;

  /*################################################################################################
   * Public destructor
   *##############################################################################################*/

  ~MwCASField() = default;

  /*################################################################################################
   * Public operators
   *##############################################################################################*/

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
    if constexpr (std::is_same_v<T, uint64_t>) {
      return target_bit_arr_;
    } else {
      return CASTargetConverter<T>{target_bit_arr_}.target_data;
    }
  }

 private:
  /*################################################################################################
   * Internal member variables
   *##############################################################################################*/

  /// An actual target data
  uint64_t target_bit_arr_ : 63;

  /// Representing whether this field contains a MwCAS descriptor
  uint64_t mwcas_flag_ : 1;
};

// CAS target words must be one word
static_assert(sizeof(MwCASField) == kWordSize);

}  // namespace dbgroup::atomic::mwcas::component
