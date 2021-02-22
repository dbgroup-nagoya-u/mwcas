// Copyright (c) Database Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <atomic>
#include <utility>
#include <vector>

#include "casn/mwcas_descriptor.hpp"
#include "gc/epoch_based_gc.hpp"

namespace dbgroup::atomic
{
using ::dbgroup::gc::EpochBasedGC;

/**
 * @brief A class of descriptor to manage Restricted Double-Compare Single-Swap operation.
 *
 */
class MwCASManager
{
 private:
  /*################################################################################################
   * Internal member variables
   *##############################################################################################*/

  EpochBasedGC<mwcas::MwCASDescriptor> gc_;

 public:
  /*################################################################################################
   * Public constructors/destructors
   *##############################################################################################*/

  explicit MwCASManager(const size_t gc_interval_micro_sec = 1E5) : gc_{gc_interval_micro_sec} {}

  ~MwCASManager() = default;

  MwCASManager(const MwCASManager &) = delete;
  MwCASManager &operator=(const MwCASManager &obj) = delete;
  MwCASManager(MwCASManager &&) = default;
  MwCASManager &operator=(MwCASManager &&) = default;

  /*################################################################################################
   * Public utility functions
   *##############################################################################################*/

  bool
  MwCAS(std::vector<mwcas::MwCASEntry> &&entries)
  {
    const auto guard = gc_.CreateEpochGuard();

    auto desc = new mwcas::MwCASDescriptor{std::move(entries)};
    const auto success = desc->CASN();
    gc_.AddGarbage(desc);

    return success;
  }

  template <class T>
  T
  ReadMwCASField(const void *addr)
  {
    const auto guard = gc_.CreateEpochGuard();

    auto target_addr = const_cast<void *>(addr);
    auto read_val = mwcas::RDCSSDescriptor::ReadRDCSSField<mwcas::MwCASField>(target_addr);
    while (read_val.IsMwCASDescriptor()) {
      auto desc = reinterpret_cast<mwcas::MwCASDescriptor *>(read_val.GetTargetData<uintptr_t>());
      desc->CASN();
      read_val = mwcas::RDCSSDescriptor::ReadRDCSSField<mwcas::MwCASField>(target_addr);
    }
    return read_val.GetTargetData<T>();
  }
};

}  // namespace dbgroup::atomic
