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

/**
 * @brief A class of descriptor to manage Restricted Double-Compare Single-Swap operation.
 *
 */
class MwCASManager
{
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
    thread_local MwCASDescriptor desc;
    desc = MwCASDescriptor{};

    return &desc;
  }

  bool
  MwCAS(MwCASDescriptor *desc)
  {
    return desc->CASN();
  }

  template <class T>
  T
  ReadMwCASField(const void *addr)
  {
    const auto target_addr = static_cast<const std::atomic<MwCASField> *>(addr);

    MwCASField target_word;
    do {
      target_word = target_addr->load(mwcas::mo_relax);
    } while (target_word.IsMwCASDescriptor());

    return target_word.GetTargetData<T>();
  }
};

}  // namespace dbgroup::atomic
