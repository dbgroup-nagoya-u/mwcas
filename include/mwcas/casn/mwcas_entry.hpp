// Copyright (c) Database Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <atomic>

#include "common.hpp"
#include "mwcas_field.hpp"
#include "rdcss_field.hpp"

namespace dbgroup::atomic::mwcas
{
/**
 * @brief A class to represent an RDCSS target word.
 *
 */
class alignas(kCacheLineSize / 2) MwCASEntry
{
 private:
  /*################################################################################################
   * Internal member variables
   *##############################################################################################*/

  RDCSSField mwcas_desc_;

 public:
  /*################################################################################################
   * Public member variables
   *##############################################################################################*/

  std::atomic<RDCSSField> *addr;

  RDCSSField old_val;

  RDCSSField new_val;

  /*################################################################################################
   * Public constructors/destructors
   *##############################################################################################*/

  constexpr MwCASEntry() : addr{nullptr} {}

  template <class T>
  constexpr MwCASEntry(  //
      void *addr,
      const T old_v,
      const T new_v,
      void *desc_addr)
      : mwcas_desc_{MwCASField{reinterpret_cast<uintptr_t>(desc_addr), true}},
        addr{static_cast<std::atomic<RDCSSField> *>(addr)},
        old_val{old_v},
        new_val{new_v}
  {
  }

  ~MwCASEntry() = default;

  MwCASEntry(const MwCASEntry &) = default;
  MwCASEntry &operator=(const MwCASEntry &obj) = default;
  MwCASEntry(MwCASEntry &&) = default;
  MwCASEntry &operator=(MwCASEntry &&) = default;

  /*################################################################################################
   * Public utility functions
   *##############################################################################################*/

  RDCSSField
  RDCSS()
  {
    const auto desc_addr = RDCSSField{reinterpret_cast<uintptr_t>(this), true};

    // empbed a target descriptor in a corresponding address
    RDCSSField loaded_word;
    do {
      loaded_word = old_val;
      while (!addr->compare_exchange_weak(loaded_word, desc_addr, mo_relax)
             && loaded_word == old_val) {
        // weak CAS may fail although it can perform
      }
    } while (loaded_word.IsRDCSSDescriptor());

    if (loaded_word == old_val) {
      // embedding succeeds, so complete RDCSS
      auto expected = desc_addr;
      while (!addr->compare_exchange_weak(expected, mwcas_desc_, mo_relax)
             && expected == desc_addr) {
        // weak CAS may fail although it can perform
      }
    }

    return loaded_word;
  }
};

static_assert(sizeof(MwCASEntry) == kCacheLineSize / 2);

}  // namespace dbgroup::atomic::mwcas
