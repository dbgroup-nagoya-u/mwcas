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

  constexpr MwCASTarget() : addr{}, old_val{}, new_val{} {}

  template <class T>
  constexpr MwCASTarget(  //
      void *addr,
      const T old_v,
      const T new_v)
      : addr{static_cast<std::atomic<MwCASField> *>(addr)}, old_val{old_v}, new_val{new_v}
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
   * Public member variables
   *##############################################################################################*/

  /// A target memory address
  std::atomic<MwCASField> *addr;

  /// An expected value of a target field
  MwCASField old_val;

  /// An inserting value into a target field
  MwCASField new_val;
};

}  // namespace dbgroup::atomic::mwcas::component
