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
using mwcas::RDCSSField;

/**
 * @brief A class of descriptor to manage Restricted Double-Compare Single-Swap operation.
 *
 */
class MwCASManager
{
 private:
  /*################################################################################################
   * Internal utility functions
   *##############################################################################################*/

  template <class T>
  static T
  ReadRDCSSField(void *addr)
  {
    const auto target_addr = static_cast<std::atomic<RDCSSField> *>(addr);

    RDCSSField target_word;
    do {
      target_word = target_addr->load(mwcas::mo_relax);
    } while (target_word.IsRDCSSDescriptor());

    return target_word.GetTargetData<T>();
  }

 public:
  /*################################################################################################
   * Public constructors/destructors
   *##############################################################################################*/

  constexpr MwCASManager() {}

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
    const auto success = desc->CASN();

    return success;
  }

  template <class T>
  T
  ReadMwCASField(void *addr)
  {
    MwCASField read_val;
    do {
      read_val = ReadRDCSSField<MwCASField>(addr);
    } while (read_val.IsMwCASDescriptor());

    return read_val.GetTargetData<T>();
  }
};

}  // namespace dbgroup::atomic
