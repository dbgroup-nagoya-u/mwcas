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

#ifndef MWCAS_MWCAS_COMPONENT_MWCAS_FIELD_H_
#define MWCAS_MWCAS_COMPONENT_MWCAS_FIELD_H_

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

  /**
   * @brief Construct an empty field for MwCAS.
   *
   */
  constexpr MwCASField() : target_bit_arr_{}, mwcas_flag_{false} {}

  /**
   * @brief Construct a MwCAS field with given data.
   *
   * @tparam T a target class to be embedded.
   * @param target_data target data to be embedded.
   * @param is_mwcas_descriptor a flag to indicate this field contains a descriptor.
   */
  template <class T>
  constexpr MwCASField(  //
      const T target_data,
      const bool is_mwcas_descriptor = false)
      : target_bit_arr_{ConvertToUint64(target_data)}, mwcas_flag_{is_mwcas_descriptor}
  {
    // static check to validate MwCAS targets
    static_assert(sizeof(T) == kWordSize);
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(std::is_copy_constructible_v<T>);
    static_assert(std::is_move_constructible_v<T>);
    static_assert(std::is_copy_assignable_v<T>);
    static_assert(std::is_move_assignable_v<T>);
    static_assert(CanMwCAS<T>());
  }

  constexpr MwCASField(const MwCASField &) = default;
  constexpr MwCASField &operator=(const MwCASField &obj) = default;
  constexpr MwCASField(MwCASField &&) = default;
  constexpr MwCASField &operator=(MwCASField &&) = default;

  /*################################################################################################
   * Public destructor
   *##############################################################################################*/

  /**
   * @brief Destroy the MwCASField object.
   *
   */
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

  /**
   * @retval true if this field contains a descriptor.
   * @retval false otherwise.
   */
  constexpr bool
  IsMwCASDescriptor() const
  {
    return mwcas_flag_;
  }

  /**
   * @tparam T an expected class of data.
   * @return data retained in this field.
   */
  template <class T>
  constexpr T
  GetTargetData() const
  {
    if constexpr (std::is_same_v<T, uint64_t>) {
      return target_bit_arr_;
    } else if constexpr (std::is_pointer_v<T>) {
      return reinterpret_cast<T>(target_bit_arr_);
    } else {
      return CASTargetConverter<T>{target_bit_arr_}.target_data;
    }
  }

 private:
  /*################################################################################################
   * Internal utility functions
   *##############################################################################################*/

  /**
   * @brief Conver given data into uint64_t.
   *
   * @tparam T a class of given data.
   * @param data data to be converted.
   * @return data converted to uint64_t.
   */
  template <class T>
  constexpr uint64_t
  ConvertToUint64(const T data)
  {
    if constexpr (std::is_same_v<T, uint64_t>) {
      return data;
    } else if constexpr (std::is_pointer_v<T>) {
      return reinterpret_cast<uint64_t>(data);
    } else {
      return CASTargetConverter{data}.converted_data;
    }
  }

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

#endif  // MWCAS_MWCAS_COMPONENT_MWCAS_FIELD_H_
