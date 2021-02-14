// Copyright (c) Database Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <atomic>

#include "common.hpp"
#include "rdcss_target_word.hpp"

namespace dbgroup::atomic::mwcas
{
/**
 * @brief A class of descriptor to manage Restricted Double-Compare Single-Swap operation.
 *
 */
class RDCSSDescriptor
{
 private:
  std::atomic<RDCSSTargetWord> *addr_1_;

  RDCSSTargetWord old_1_;

  std::atomic<RDCSSTargetWord> *addr_2_;

  RDCSSTargetWord old_2_;

  RDCSSTargetWord new_2_;

 public:
  RDCSSDescriptor() {}

  ~RDCSSDescriptor() {}

  template <class T>
  void
  SetFirstTarget(  //
      const T *target_addr,
      const T old_target)
  {
    addr_1_ = static_cast<RDCSSTargetWord *>(target_addr);
    old_1_ = RDCSSTargetWord{old_target};
  }

  template <class T>
  void
  SetSecondTarget(  //
      const T *target_addr,
      const T old_target,
      const T new_target)
  {
    addr_2_ = static_cast<RDCSSTargetWord *>(target_addr);
    old_2_ = RDCSSTargetWord{old_target};
    new_2_ = RDCSSTargetWord{new_target};
  }

  static RDCSSTargetWord
  PerformRDCSS(RDCSSDescriptor *desc)
  {
    RDCSSTargetWord loaded_word;
    do {
      const auto desc_addr = RDCSSTargetWord{reinterpret_cast<uintptr_t>(desc), true};
      auto old_2 = desc->old_2_;
      desc->addr_2_->compare_exchange_weak(old_2, desc_addr);
      loaded_word = desc->addr_2_->load();
      if (loaded_word.IsRDCSSDescriptor()) {
        auto loaded_desc =
            reinterpret_cast<RDCSSDescriptor *>(loaded_word.GetTargetData<uintptr_t>());
        CompleteRDCSS(loaded_desc);
      }
    } while (loaded_word.IsRDCSSDescriptor());

    if (loaded_word == desc->old_2_) {
      CompleteRDCSS(desc);
    }

    return loaded_word;
  }

  static void
  CompleteRDCSS(RDCSSDescriptor *desc)
  {
    auto desc_addr = RDCSSTargetWord{reinterpret_cast<uintptr_t>(desc), true};
    if (desc->addr_1_->load() == desc->old_1_) {
      desc->addr_2_->compare_exchange_weak(desc_addr, desc->new_2_);
    } else {
      desc->addr_2_->compare_exchange_weak(desc_addr, desc->old_2_);
    }
  }

  template <class T>
  static T
  ReadRDCSSField(void *addr)
  {
    RDCSSTargetWord *target_word;
    do {
      target_word = static_cast<RDCSSTargetWord *>(addr);
      if (target_word->IsRDCSSDescriptor()) {
        auto loaded_desc =
            reinterpret_cast<RDCSSDescriptor *>(target_word->GetTargetData<uintptr_t>());
        CompleteRDCSS(loaded_desc);
      }
    } while (target_word->IsRDCSSDescriptor());

    return target_word->GetTargetData<T>();
  }
};

}  // namespace dbgroup::atomic::mwcas
