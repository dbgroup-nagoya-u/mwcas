// Copyright (c) Database Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <atomic>

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

  static RDCSSField
  CompleteRDCSS(RDCSSDescriptor *desc)
  {
    auto desc_addr = RDCSSField{reinterpret_cast<uintptr_t>(desc), true};
    if (desc->addr_1_->load() == desc->old_1_) {
      // because the first target remains, try completion of RDCSS
      if (desc->addr_2_->compare_exchange_weak(desc_addr, desc->new_2_)) {
        return desc->new_2_;
      } else {
        // a target field has been already swapped by other threads
        return desc_addr;
      }
    } else {
      // because other threads modify the first target, abort RDCSS
      if (desc->addr_2_->compare_exchange_weak(desc_addr, desc->old_2_)) {
        return desc->old_2_;
      } else {
        // a target field has been already swapped by other threads
        return desc_addr;
      }
    }
  }

 public:
  /*################################################################################################
   * Public constructors/destructors
   *##############################################################################################*/

  RDCSSDescriptor() {}

  template <class T1, class T2>
  RDCSSDescriptor(  //
      void *addr_1,
      const T1 old_1,
      void *addr_2,
      const T2 old_2,
      const T2 new_2)
      : addr_1_{static_cast<std::atomic<RDCSSField> *>(addr_1)},
        old_1_{old_1},
        addr_2_{static_cast<std::atomic<RDCSSField> *>(addr_2)},
        old_2_{old_2},
        new_2_{new_2}
  {
  }

  ~RDCSSDescriptor() {}

  /*################################################################################################
   * Public getters/setters
   *##############################################################################################*/

  template <class T>
  void
  SetFirstTarget(  //
      void *target_addr,
      const T old_target)
  {
    addr_1_ = static_cast<std::atomic<RDCSSField> *>(target_addr);
    old_1_ = RDCSSField{old_target};
  }

  template <class T>
  void
  SetSecondTarget(  //
      void *target_addr,
      const T old_target,
      const T new_target)
  {
    addr_2_ = static_cast<std::atomic<RDCSSField> *>(target_addr);
    old_2_ = RDCSSField{old_target};
    new_2_ = RDCSSField{new_target};
  }

  /*################################################################################################
   * Public utility functions
   *##############################################################################################*/

  RDCSSField
  RDCSS()
  {
    const auto desc_addr = RDCSSField{reinterpret_cast<uintptr_t>(this), true};

    // empbed a target descriptor in a corresponding address
    auto loaded_word = old_2_;
    if (addr_2_->compare_exchange_weak(loaded_word, desc_addr)) {
      // if embedding succeeds, try completion of RDCSS
      return CompleteRDCSS(this);
    } else if (loaded_word.IsRDCSSDescriptor()) {
      // if another descriptor is embedded, complete it first
      auto loaded_desc =
          reinterpret_cast<RDCSSDescriptor *>(loaded_word.GetTargetData<uintptr_t>());
      return CompleteRDCSS(loaded_desc);
    } else {
      // if a target field is updated, return it
      return loaded_word;
    }
  }

  template <class T>
  static T
  ReadRDCSSField(void *addr)
  {
    // read a target address atomically
    auto target_word = static_cast<std::atomic<RDCSSField> *>(addr)->load();

    if (target_word.IsRDCSSDescriptor()) {
      // if a read value is a descriptor, complete it first
      auto loaded_desc =
          reinterpret_cast<RDCSSDescriptor *>(target_word.GetTargetData<uintptr_t>());
      return CompleteRDCSS(loaded_desc).GetTargetData<T>();
    } else {
      return target_word.GetTargetData<T>();
    }
  }
};

}  // namespace dbgroup::atomic::mwcas
