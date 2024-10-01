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

#ifndef MWCAS_COMPONENT_MWCAS_FIELD_HPP
#define MWCAS_COMPONENT_MWCAS_FIELD_HPP

// C++ standard libraries
#include <atomic>
#include <bit>
#include <cstdint>
#include <cstring>
#include <type_traits>

// local sources
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
  /*####################################################################################
   * Public constructors and assignment operators
   *##################################################################################*/

  /**
   * @brief Construct an empty field for MwCAS.
   *
   */
  constexpr MwCASField() = default;

  /**
   * @brief Construct a MwCAS field with given data.
   *
   * @tparam T a target class to be embedded.
   * @param target_data target data to be embedded.
   * @param is_mwcas_descriptor a flag to indicate this field contains a descriptor.
   */
  template <class T>
  constexpr explicit MwCASField(  //
      T target_data,
      bool is_mwcas_descriptor = false)
      : target_bit_arr_{std::bit_cast<uint64_t>(target_data)}, mwcas_flag_{is_mwcas_descriptor}
  {
    // static check to validate MwCAS targets
    static_assert(CanMwCAS<T>());
  }

  constexpr MwCASField(const MwCASField &) = default;
  constexpr MwCASField(MwCASField &&) noexcept = default;

  constexpr auto operator=(const MwCASField &obj) -> MwCASField & = default;
  constexpr auto operator=(MwCASField &&) noexcept -> MwCASField & = default;

  /*####################################################################################
   * Public destructor
   *##################################################################################*/

  /**
   * @brief Destroy the MwCASField object.
   *
   */
  ~MwCASField() = default;

  /*####################################################################################
   * Public operators
   *##################################################################################*/

  constexpr auto
  operator==(                       //
      const MwCASField &obj) const  //
      -> bool
  {
    return std::bit_cast<uint64_t>(*this) == std::bit_cast<uint64_t>(obj);
  }

  constexpr auto
  operator!=(                       //
      const MwCASField &obj) const  //
      -> bool
  {
    return std::bit_cast<uint64_t>(*this) != std::bit_cast<uint64_t>(obj);
  }

  /*####################################################################################
   * Public getters/setters
   *##################################################################################*/

  /**
   * @retval true if this field contains a descriptor.
   * @retval false otherwise.
   */
  [[nodiscard]] constexpr auto
  IsMwCASDescriptor() const  //
      -> bool
  {
    return mwcas_flag_;
  }

  /**
   * @tparam T an expected class of data.
   * @return data retained in this field.
   */
  template <class T>
  [[nodiscard]] constexpr auto
  GetTargetData() const  //
      -> T
  {
    if constexpr (std::is_same_v<T, uint64_t>) {
      return target_bit_arr_;
    } else {
      return std::bit_cast<T>(target_bit_arr_);
    }
  }

 private:
  /*####################################################################################
   * Internal utility functions
   *##################################################################################*/

  /**
   * @brief Conver given data into uint64_t.
   *
   * @tparam T a class of given data.
   * @param data data to be converted.
   * @return data converted to uint64_t.
   */
  template <class T>
  constexpr auto
  ConvertToUint64(   //
      const T data)  //
      -> uint64_t
  {
    if constexpr (std::is_same_v<T, uint64_t>) {
      return data;
    } else {
      return std::bit_cast<uint64_t>(data);
    }
  }

  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

  /// An actual target data
  uint64_t target_bit_arr_ : 63 {};

  /// Representing whether this field contains a MwCAS descriptor
  uint64_t mwcas_flag_ : 1 {};
};

// CAS target words must be one word
static_assert(sizeof(MwCASField) == kWordSize);

}  // namespace dbgroup::atomic::mwcas::component

#endif  // MWCAS_COMPONENT_MWCAS_FIELD_HPP
