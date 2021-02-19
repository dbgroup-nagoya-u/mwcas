// Copyright (c) Database Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <atomic>

#include "cas_n_field.hpp"
#include "common.hpp"
#include "mwcas_field.hpp"
#include "rdcss_descriptor.hpp"

namespace dbgroup::atomic::mwcas
{
/**
 * @brief A class to represent an RDCSS target word.
 *
 */
class alignas(kCacheLineSize) MwCASEntry
{
 public:
  /*################################################################################################
   * Internal member variables
   *##############################################################################################*/

  std::atomic<MwCASField> *addr;

  MwCASField old_val;

  MwCASField new_val;

  RDCSSDescriptor rdcss_desc;

  /*################################################################################################
   * Public constructors/destructors
   *##############################################################################################*/

  template <class T>
  constexpr MwCASEntry(  //
      void *addr,
      const T old_v,
      const T new_v)
      : addr{static_cast<std::atomic<MwCASField> *>(addr)},
        old_val{old_v},
        new_val{new_v},
        rdcss_desc{MwCASStatus::kUndecided, addr, old_v}
  {
  }

  ~MwCASEntry() = default;

  MwCASEntry(const MwCASEntry &) = delete;
  MwCASEntry &operator=(const MwCASEntry &obj) = delete;
  MwCASEntry(MwCASEntry &&) = default;
  MwCASEntry &operator=(MwCASEntry &&) = default;
};

static_assert(sizeof(MwCASEntry) == kCacheLineSize);

}  // namespace dbgroup::atomic::mwcas
