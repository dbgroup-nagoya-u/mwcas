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

  explicit MwCASDescriptor(std::vector<MwCASEntry> &&entries)
      : status_{MwCASStatus::kUndecided}, entries_{std::move(entries)}
  {
    const auto desc_addr = CASNField{reinterpret_cast<uintptr_t>(this), true};
    for (auto &&entry : entries_) {
      entry.rdcss_desc.SetMwCASDescriptorInfo(&status_, desc_addr);
    }
  }

  ~MwCASDescriptor() = default;

  MwCASDescriptor(const MwCASDescriptor &) = delete;
  MwCASDescriptor &operator=(const MwCASDescriptor &obj) = delete;
  MwCASDescriptor(MwCASDescriptor &&) = default;
  MwCASDescriptor &operator=(MwCASDescriptor &&) = default;

  /*################################################################################################
   * Public utility functions
   *##############################################################################################*/

  bool
  CASN()
  {
    auto current_status = status_.load();
    if (current_status == MwCASStatus::kUndecided) {
      auto new_status = MwCASStatus::kSuccess;
      for (size_t i = 0; i < entries_.size(); ++i) {
        MwCASField expected;
        do {
          expected = MwCASField{entries_[i].rdcss_desc.RDCSS()};
          if (expected.IsMwCASDescriptor()) {
            auto desc = reinterpret_cast<MwCASDescriptor *>(expected.GetTargetData<uintptr_t>());
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
    const auto desc_word = MwCASField{reinterpret_cast<uintptr_t>(this), true};
    for (auto &&entry : entries_) {
      const auto desired = (success) ? entry.new_val : entry.old_val;
      auto desc = desc_word;
      while (!entry.addr->compare_exchange_weak(desc, desired) && desc == desc_word) {
        // weak CAS may fail although it can perform
      }
      if (success && desired == entry.old_val) {
        int tmp = 0;
      }
    }

    return success;
  }
};

}  // namespace dbgroup::atomic::mwcas
