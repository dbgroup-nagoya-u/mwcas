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

  /*################################################################################################
   * Internal utility functions
   *##############################################################################################*/

  static void
  CompleteRDCSS(RDCSSDescriptor *desc)
  {
    const auto desc_addr = RDCSSField{reinterpret_cast<uintptr_t>(desc), true};
    const auto desired =
        (desc->addr_1_->load(mo_relax) == desc->old_1_) ? desc->new_2_ : desc->old_2_;

    auto expected = desc_addr;
    while (!desc->addr_2_->compare_exchange_weak(expected, desired, mo_relax)
           && expected == desc_addr) {
      // weak CAS may fail although it can perform
    }
  }

 public:
  /*################################################################################################
   * Public constructors/destructors
   *##############################################################################################*/

  constexpr RDCSSDescriptor() : addr_1_{nullptr}, addr_2_{nullptr} {}

  template <class T1, class T2, class T3>
  RDCSSDescriptor(  //
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

  ~RDCSSDescriptor() {}

  RDCSSDescriptor(const RDCSSDescriptor &) = delete;
  RDCSSDescriptor &operator=(const RDCSSDescriptor &obj) = delete;
  RDCSSDescriptor(RDCSSDescriptor &&) = default;
  RDCSSDescriptor &operator=(RDCSSDescriptor &&) = default;

  /*################################################################################################
   * Public utility functions
   *##############################################################################################*/

  RDCSSField
  RDCSS()
  {
    const auto desc_addr = RDCSSField{reinterpret_cast<uintptr_t>(this), true};

    do {
      // empbed a target descriptor in a corresponding address
      auto loaded_word = old_2_;
      while (!addr_2_->compare_exchange_weak(loaded_word, desc_addr, mo_relax)
             && loaded_word == old_2_) {
        // weak CAS may fail although it can perform
      }

      if (loaded_word == old_2_) {
        // if embedding succeeds, try completion of RDCSS
        CompleteRDCSS(this);
        return loaded_word;
      } else if (!loaded_word.IsRDCSSDescriptor()) {
        // if a target word is updated, return it
        return loaded_word;
      }

      // if another descriptor is embedded, complete it first
      auto loaded_desc =
          reinterpret_cast<RDCSSDescriptor *>(loaded_word.GetTargetData<uintptr_t>());
      CompleteRDCSS(loaded_desc);
    } while (true);
  }

  template <class T>
  static T
  ReadRDCSSField(void *addr)
  {
    // read a target address atomically
    auto target_addr = static_cast<std::atomic<RDCSSField> *>(addr);
    auto target_word = target_addr->load(mo_relax);

    while (target_word.IsRDCSSDescriptor()) {
      auto loaded_desc =
          reinterpret_cast<RDCSSDescriptor *>(target_word.GetTargetData<uintptr_t>());
      CompleteRDCSS(loaded_desc);
      target_word = target_addr->load(mo_relax);
    }

    return target_word.GetTargetData<T>();
  }
};

}  // namespace dbgroup::atomic::mwcas
