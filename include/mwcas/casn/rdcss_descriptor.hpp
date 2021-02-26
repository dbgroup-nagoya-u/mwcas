// Copyright (c) Database Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <atomic>
#include <utility>

#include "common.hpp"
#include "rdcss_field.hpp"

namespace dbgroup::atomic::mwcas
{
/**
 * @brief A class of descriptor to manage Restricted Double-Compare Single-Swap operation.
 *
 */
class RDCSSDescriptor
{
 private:
  /*################################################################################################
   * Internal member variables
   *##############################################################################################*/

  std::atomic<RDCSSField> *addr_1_;

  RDCSSField old_1_;

  std::atomic<RDCSSField> *addr_2_;

  RDCSSField old_2_;

  RDCSSField new_2_;

 public:
  /*################################################################################################
   * Public constructors/destructors
   *##############################################################################################*/

  constexpr RDCSSDescriptor() : addr_1_{nullptr}, addr_2_{nullptr} {}

  template <class T1, class T2, class T3>
  constexpr RDCSSDescriptor(  //
      void *addr_1,
      const T1 old_1,
      void *addr_2,
      const T2 old_2,
      const T3 new_2)
      : addr_1_{static_cast<std::atomic<RDCSSField> *>(addr_1)},
        old_1_{old_1},
        addr_2_{static_cast<std::atomic<RDCSSField> *>(addr_2)},
        old_2_{old_2},
        new_2_{new_2}
  {
  }

  ~RDCSSDescriptor() = default;

  RDCSSDescriptor(const RDCSSDescriptor &) = default;
  RDCSSDescriptor &operator=(const RDCSSDescriptor &obj) = default;
  RDCSSDescriptor(RDCSSDescriptor &&) = default;
  RDCSSDescriptor &operator=(RDCSSDescriptor &&) = default;

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
      loaded_word = old_2_;
      while (!addr_2_->compare_exchange_weak(loaded_word, desc_addr, mo_relax)
             && loaded_word == old_2_) {
        // weak CAS may fail although it can perform
      }
    } while (loaded_word.IsRDCSSDescriptor());

    if (loaded_word == old_2_) {
      // embedding succeeds, so complete RDCSS
      const auto desired = (addr_1_->load(mo_relax) == old_1_) ? new_2_ : old_2_;
      auto expected = desc_addr;
      while (!addr_2_->compare_exchange_weak(expected, desired, mo_relax)
             && expected == desc_addr) {
        // weak CAS may fail although it can perform
      }
    }

    return loaded_word;
  }

  template <class T>
  static T
  ReadRDCSSField(void *addr)
  {
    const auto target_addr = static_cast<std::atomic<RDCSSField> *>(addr);

    RDCSSField target_word;
    do {
      target_word = target_addr->load(mo_relax);
    } while (target_word.IsRDCSSDescriptor());

    return target_word.GetTargetData<T>();
  }
};

}  // namespace dbgroup::atomic::mwcas
