// Copyright (c) Database Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <array>
#include <atomic>
#include <utility>

#include "common.hpp"
#include "mwcas_entry.hpp"
#include "mwcas_field.hpp"

namespace dbgroup::atomic::mwcas
{
/**
 * @brief A class of descriptor to manage Restricted Double-Compare Single-Swap operation.
 *
 */
class alignas(kCacheLineSize) MwCASDescriptor
{
 private:
  /*################################################################################################
   * Internal member variables
   *##############################################################################################*/

  MwCASEntry entries_[kTargetWordNum];

  size_t word_count_;

 public:
  /*################################################################################################
   * Public constructors/destructors
   *##############################################################################################*/

  constexpr MwCASDescriptor() : word_count_{0} {}

  ~MwCASDescriptor() = default;

  MwCASDescriptor(const MwCASDescriptor &) = default;
  MwCASDescriptor &operator=(const MwCASDescriptor &obj) = default;
  MwCASDescriptor(MwCASDescriptor &&) = default;
  MwCASDescriptor &operator=(MwCASDescriptor &&) = default;

  /*################################################################################################
   * Public utility functions
   *##############################################################################################*/

  template <class T>
  bool
  AddEntry(  //
      void *addr,
      const T old_v,
      const T new_v)
  {
    if (word_count_ == kTargetWordNum) {
      return false;
    } else {
      entries_[word_count_++] = MwCASEntry{addr, old_v, new_v};
      return true;
    }
  }

  bool
  CASN()
  {
    const auto desc_addr = reinterpret_cast<uintptr_t>(this);
    const auto desc_word = MwCASField{desc_addr, true};

    // serialize MwCAS operations by embedding a descriptor
    auto mwcas_success = true;
    size_t embedded_count = 0;
    for (size_t i = 0; i < word_count_; ++i, ++embedded_count) {
      // embed a MwCAS decriptor
      MwCASField loaded_word;
      do {
        loaded_word = entries_[i].old_val;
        while (!entries_[i].addr->compare_exchange_weak(loaded_word, desc_word, mo_relax)
               && loaded_word == entries_[i].old_val) {
          // weak CAS may fail although it can perform
        }
      } while (loaded_word.IsMwCASDescriptor());

      if (loaded_word != entries_[i].old_val) {
        // if a target filed has been already updated, MwCAS is failed
        mwcas_success = false;
        break;
      }
    }

    // complete MwCAS
    for (size_t i = 0; i < embedded_count; ++i) {
      const auto desired = (mwcas_success) ? entries_[i].new_val : entries_[i].old_val;
      auto desc = desc_word;
      while (!entries_[i].addr->compare_exchange_weak(desc, desired, mo_relax)
             && desc == desc_word) {
        // weak CAS may fail although it can perform
      }
    }

    return mwcas_success;
  }
};

}  // namespace dbgroup::atomic::mwcas
