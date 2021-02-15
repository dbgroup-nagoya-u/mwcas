// Copyright (c) Database Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <atomic>
#include <vector>

#include "common.hpp"
#include "mwcas_entry.hpp"
#include "mwcas_field.hpp"
#include "rdcss_field.hpp"

namespace dbgroup::atomic::mwcas
{
/**
 * @brief A class of descriptor to manage Restricted Double-Compare Single-Swap operation.
 *
 */
class MwCASDescriptor
{
 private:
  /*################################################################################################
   * Internal member variables
   *##############################################################################################*/

  std::atomic<MwCASStatus> status_;

  std::vector<MwCASEntry> entries_;

 public:
  /*################################################################################################
   * Public constructors/destructors
   *##############################################################################################*/

  MwCASDescriptor() : status_{MwCASStatus::kUndecided} {}

  ~MwCASDescriptor() = default;

  /*################################################################################################
   * Public getters/setters
   *##############################################################################################*/

  template <class T>
  void
  AddEntry(  //
      void* addr,
      const T old_val,
      const T new_val)
  {
    auto&& old_field = MwCASField{old_val};
    auto&& desc_addr = MwCASField{reinterpret_cast<uintptr_t>(this), true};
    auto&& rdcss_desc =
        RDCSSDescriptor{&status_, MwCASStatus::kUndecided, addr, old_field, desc_addr};

    entries_.emplace_back(MwCASEntry{addr, old_val, new_val, rdcss_desc});
  }
};

}  // namespace dbgroup::atomic::mwcas
