// Copyright (c) Database Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <atomic>
#include <utility>
#include <vector>

#include "common.hpp"
#include "gc/epoch_based_gc.hpp"
#include "mwcas_descriptor.hpp"

namespace dbgroup::atomic::mwcas
{
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

  gc::epoch::EpochBasedGC<MwCASDescriptor> gc_;

 public:
  /*################################################################################################
   * Public constructors/destructors
   *##############################################################################################*/

  MwCASManager() {}

  ~MwCASManager() = default;

  /*################################################################################################
   * Public utility functions
   *##############################################################################################*/

  bool
  MwCAS(std::vector<MwCASEntry> &&entries)
  {
    const auto guard = gc_.CreateEpochGuard();

    auto desc = new MwCASDescriptor{std::move(entries)};
    const auto success = desc->CASN();

    gc_.AddGarbage(guard, desc);

    return success;
  }
};

}  // namespace dbgroup::atomic::mwcas