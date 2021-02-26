// Copyright (c) Database Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <atomic>

#include "common.hpp"
#include "mwcas_field.hpp"
#include "rdcss_descriptor.hpp"
#include "rdcss_field.hpp"

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

  std::atomic<RDCSSField> *addr;

  RDCSSField old_val;

  RDCSSField new_val;

  RDCSSDescriptor rdcss_desc;

  /*################################################################################################
   * Public constructors/destructors
   *##############################################################################################*/

  constexpr MwCASEntry() : addr{nullptr} {}

  template <class T>
  constexpr MwCASEntry(  //
      void *addr,
      const T old_v,
      const T new_v,
      void *desc_addr,
      void *status_addr)
      : addr{static_cast<std::atomic<RDCSSField> *>(addr)},
        old_val{old_v},
        new_val{new_v},
        rdcss_desc{status_addr, MwCASStatus::kUndecided, addr, old_v,
                   MwCASField{reinterpret_cast<uintptr_t>(desc_addr), true}}
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
