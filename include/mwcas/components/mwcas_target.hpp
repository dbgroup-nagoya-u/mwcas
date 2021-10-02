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

#include "mwcas_field.hpp"

namespace dbgroup::atomic::mwcas::component
{
/**
 * @brief A class to represent a MwCAS target.
 *
 */
class MwCASTarget
{
 public:
  /*################################################################################################
   * Public constructors and assignment operators
   *##############################################################################################*/

  constexpr MwCASTarget() : addr_{}, old_val_{}, new_val_{} {}

  template <class T>
  constexpr MwCASTarget(  //
      void *addr,
      const T old_val,
      const T new_val)
      : addr_{static_cast<std::atomic<MwCASField> *>(addr)}, old_val_{old_val}, new_val_{new_val}
  {
  }

  constexpr MwCASTarget(const MwCASTarget &) = default;
  constexpr MwCASTarget &operator=(const MwCASTarget &obj) = default;
  constexpr MwCASTarget(MwCASTarget &&) = default;
  constexpr MwCASTarget &operator=(MwCASTarget &&) = default;

  /*################################################################################################
   * Public destructor
   *##############################################################################################*/

  ~MwCASTarget() = default;

  /*################################################################################################
   * Public getters/setters
   *##############################################################################################*/

  constexpr MwCASField
  GetOldVal() const
  {
    return old_val_;
  }

  constexpr MwCASField
  GetCompleteVal(const bool mwcas_success) const
  {
    return (mwcas_success) ? new_val_ : old_val_;
  }

  /*################################################################################################
   * Public utility functions
   *##############################################################################################*/

  MwCASField
  CAS(  //
      const MwCASField expected,
      const MwCASField desired)
  {
    MwCASField current = expected;
    while (!addr_->compare_exchange_weak(current, desired, mo_relax) && current == expected) {
      // weak CAS may fail even if it can perform
    }

    return current;
  }

 private:
  /*################################################################################################
   * Internal member variables
   *##############################################################################################*/

  /// A target memory address
  std::atomic<MwCASField> *addr_;

  /// An expected value of a target field
  MwCASField old_val_;

  /// An inserting value into a target field
  MwCASField new_val_;
};

}  // namespace dbgroup::atomic::mwcas::component
