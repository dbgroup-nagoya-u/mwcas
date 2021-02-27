// Copyright (c) Database Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <atomic>

#include "mwcas_field.hpp"

namespace dbgroup::atomic::mwcas
{
/**
 * @brief A class to represent a MwCAS target.
 *
 */
class MwCASTarget
{
 public:
  /*################################################################################################
   * Public member variables
   *##############################################################################################*/

  /// A target memory address
  std::atomic<MwCASField> *addr;

  /// An expected value of a target field
  MwCASField old_val;

  /// An inserting value into a target field
  MwCASField new_val;

  /*################################################################################################
   * Public constructors/destructors
   *##############################################################################################*/

  constexpr MwCASTarget() : addr{nullptr} {}

  template <class T>
  constexpr MwCASTarget(  //
      void *addr,
      const T old_v,
      const T new_v)
      : addr{static_cast<std::atomic<MwCASField> *>(addr)}, old_val{old_v}, new_val{new_v}
  {
  }

  ~MwCASTarget() = default;

  MwCASTarget(const MwCASTarget &) = default;
  MwCASTarget &operator=(const MwCASTarget &obj) = default;
  MwCASTarget(MwCASTarget &&) = default;
  MwCASTarget &operator=(MwCASTarget &&) = default;
};

}  // namespace dbgroup::atomic::mwcas
