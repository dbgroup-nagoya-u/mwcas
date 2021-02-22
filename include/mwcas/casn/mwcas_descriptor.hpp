// Copyright (c) Database Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <atomic>
#include <utility>
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
class alignas(kCacheLineSize) MwCASDescriptor
{
 private:
  /*################################################################################################
   * Internal member variables
   *##############################################################################################*/

  std::array<MwCASEntry, kTargetWordNum> entries_;

  size_t word_count_;

  std::atomic<MwCASStatus> status_;

 public:
  /*################################################################################################
   * Public constructors/destructors
   *##############################################################################################*/

  MwCASDescriptor() : word_count_{0}, status_{MwCASStatus::kUndecided} {}

  ~MwCASDescriptor() = default;

  MwCASDescriptor(const MwCASDescriptor &) = delete;
  MwCASDescriptor &operator=(const MwCASDescriptor &obj) = delete;
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
    }

    entries_[word_count_++] = MwCASEntry{addr, old_v, new_v, this, &status_};
    return true;
  }

  bool
  CASN()
  {
    auto current_status = status_.load();
    if (current_status == MwCASStatus::kUndecided) {
      auto new_status = MwCASStatus::kSuccess;
      for (size_t i = 0; i < entries_.size(); ++i) {
        RDCSSField expected;
        do {
          expected = entries_[i].rdcss_desc.RDCSS();
          if (const auto target = expected.GetTargetData<MwCASField>();
              target.IsMwCASDescriptor()) {
            auto desc = reinterpret_cast<MwCASDescriptor *>(target.GetTargetData<uintptr_t>());
            if (desc != this) {
              desc->CASN();
              continue;
            }
          } else if (entries_[i].old_val != expected) {
            new_status = MwCASStatus::kFailed;
            goto EMBEDDING_END;
          }
        } while (false);
      }
    EMBEDDING_END:
      while (!status_.compare_exchange_weak(current_status, new_status)
             && current_status == MwCASStatus::kUndecided) {
        // weak CAS may fail although it can perform
      }
    }

    const auto success = status_ == MwCASStatus::kSuccess;
    const auto desc_word = RDCSSField{MwCASField{reinterpret_cast<uintptr_t>(this), true}};
    for (auto &&entry : entries_) {
      const auto desired = (success) ? entry.new_val : entry.old_val;
      auto desc = desc_word;
      while (!entry.addr->compare_exchange_weak(desc, desired) && desc == desc_word) {
        // weak CAS may fail although it can perform
      }
    }

    return success;
  }
};

}  // namespace dbgroup::atomic::mwcas
