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

  explicit MwCASDescriptor(std::vector<MwCASEntry> &&entries)
      : status_{MwCASStatus::kUndecided}, entries_{entries}
  {
    const auto desc_addr = MwCASField{reinterpret_cast<uintptr_t>(this), true};
    for (auto &&entry : entries_) {
      entry.rdcss_desc =
          RDCSSDescriptor{&status_, MwCASStatus::kUndecided, entry.addr, entry.old_val, desc_addr};
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
          }
        } while (false);
        if (entries_[i].old_val != expected) {
          new_status = MwCASStatus::kFailed;
          break;
        }
      }
      status_.compare_exchange_weak(current_status, new_status);
    }

    const auto success = current_status == MwCASStatus::kSuccess;
    const auto desc_uintptr = MwCASField{reinterpret_cast<uintptr_t>(this), true};
    for (auto &&entry : entries_) {
      auto desc = desc_uintptr;
      entry.addr->compare_exchange_weak(desc, (success) ? entry.new_val : entry.old_val);
    }

    return success;
  }
};

}  // namespace dbgroup::atomic::mwcas
