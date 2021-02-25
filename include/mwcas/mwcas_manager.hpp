// Copyright (c) Database Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <atomic>
#include <utility>
#include <vector>

#include "casn/mwcas_descriptor.hpp"
#include "gc/tls_based_gc.hpp"

namespace dbgroup::atomic
{
using ::dbgroup::gc::TLSBasedGC;
using mwcas::MwCASDescriptor;
using mwcas::MwCASField;
using mwcas::RDCSSDescriptor;

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

  TLSBasedGC<MwCASDescriptor> gc_;

 public:
  /*################################################################################################
   * Public constructors/destructors
   *##############################################################################################*/

  explicit MwCASManager(const size_t gc_interval_micro_sec = 1E3) : gc_{gc_interval_micro_sec} {}

  ~MwCASManager() = default;

  MwCASManager(const MwCASManager &) = delete;
  MwCASManager &operator=(const MwCASManager &obj) = delete;
  MwCASManager(MwCASManager &&) = default;
  MwCASManager &operator=(MwCASManager &&) = default;

  /*################################################################################################
   * Public utility functions
   *##############################################################################################*/

  MwCASDescriptor *
  CreateMwCASDescriptor()
  {
    return new MwCASDescriptor{};
  }

  bool
  MwCAS(MwCASDescriptor *desc)
  {
    const auto guard = gc_.CreateEpochGuard();

    const auto success = desc->CASN();
    gc_.AddGarbage(desc);

    return success;
  }

  template <class T>
  T
  ReadMwCASField(void *addr)
  {
    const auto guard = gc_.CreateEpochGuard();

    auto read_val = RDCSSDescriptor::ReadRDCSSField<MwCASField>(addr);
    while (read_val.IsMwCASDescriptor()) {
      auto desc = reinterpret_cast<MwCASDescriptor *>(read_val.GetDescAddr());
      desc->CASN();
      read_val = RDCSSDescriptor::ReadRDCSSField<MwCASField>(addr);
    }

    return read_val.GetTargetData<T>();
  }
};

}  // namespace dbgroup::atomic
