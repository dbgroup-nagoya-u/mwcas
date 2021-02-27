// Copyright (c) Database Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <atomic>

#include "common.hpp"
#include "mwcas_field.hpp"

namespace dbgroup::atomic::mwcas
{
/**
 * @brief A class to represent an RDCSS target word.
 *
 */
class MwCASEntry
{
 public:
  /*################################################################################################
   * Public member variables
   *##############################################################################################*/

  std::atomic<MwCASField> *addr;

  MwCASField old_val;

  MwCASField new_val;

  /*################################################################################################
   * Public constructors/destructors
   *##############################################################################################*/

  constexpr MwCASEntry() : addr{nullptr} {}

  template <class T>
  constexpr MwCASEntry(  //
      void *addr,
      const T old_v,
      const T new_v)
      : addr{static_cast<std::atomic<MwCASField> *>(addr)}, old_val{old_v}, new_val{new_v}
  {
  }

  ~MwCASEntry() = default;

  MwCASEntry(const MwCASEntry &) = default;
  MwCASEntry &operator=(const MwCASEntry &obj) = default;
  MwCASEntry(MwCASEntry &&) = default;
  MwCASEntry &operator=(MwCASEntry &&) = default;
};

}  // namespace dbgroup::atomic::mwcas
