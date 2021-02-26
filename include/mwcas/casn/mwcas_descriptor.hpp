// Copyright (c) Database Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <array>
#include <atomic>
#include <utility>

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

  size_t word_count_;

  std::atomic<MwCASStatus> status_;

  uintptr_t desc_addr_;

  RDCSSField desc_word_;

  std::array<MwCASEntry, kTargetWordNum> entries_;

 public:
  /*################################################################################################
   * Public constructors/destructors
   *##############################################################################################*/

  constexpr MwCASDescriptor()
      : word_count_{0},
        status_{MwCASStatus::kUndecided},
        desc_addr_{reinterpret_cast<uintptr_t>(this)},
        desc_word_{MwCASField{desc_addr_, true}}
  {
  }

  ~MwCASDescriptor() = default;

  MwCASDescriptor(const MwCASDescriptor &) = delete;
  MwCASDescriptor &operator=(const MwCASDescriptor &obj) = delete;
  MwCASDescriptor(MwCASDescriptor &&) = delete;
  MwCASDescriptor &operator=(MwCASDescriptor &&) = delete;

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
      entries_[word_count_++] = MwCASEntry{addr, old_v, new_v, this, &status_};
      return true;
    }
  }

  bool
  CASN()
  {
    auto new_status = MwCASStatus::kSuccess;
    size_t embedded_count = 0;
    for (size_t i = 0; i < word_count_; ++i, ++embedded_count) {
      // embed a MwCAS decriptor
      RDCSSField rdcss_result;
      MwCASField embedded;
      do {
        rdcss_result = entries_[i].rdcss_desc.RDCSS();
        embedded = rdcss_result.GetTargetData<MwCASField>();
      } while (embedded.IsMwCASDescriptor() && embedded.GetTargetData<uintptr_t>() != desc_addr_);

      if (!embedded.IsMwCASDescriptor() && rdcss_result != entries_[i].old_val) {
        // if a target filed has been already updated, MwCAS is failed
        new_status = MwCASStatus::kFailed;
        break;
      }
    }

    // update the status of a MwCAS descriptor
    auto current_status = MwCASStatus::kUndecided;
    while (!status_.compare_exchange_weak(current_status, new_status, mo_relax)
           && current_status == MwCASStatus::kUndecided) {
      // weak CAS may fail although it can perform
    }

    // complete MwCAS
    const auto success = new_status == MwCASStatus::kSuccess;
    for (size_t index = 0; index < embedded_count; ++index) {
      const auto desired = (success) ? entries_[index].new_val : entries_[index].old_val;
      auto desc = desc_word_;
      while (!entries_[index].addr->compare_exchange_weak(desc, desired, mo_relax)
             && desc == desc_word_) {
        // weak CAS may fail although it can perform
      }
    }

    return success;
  }
};

}  // namespace dbgroup::atomic::mwcas
