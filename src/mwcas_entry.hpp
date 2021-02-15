// Copyright (c) Database Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <atomic>

#include "common.hpp"
#include "mwcas_field.hpp"
#include "rdcss_descriptor.hpp"

namespace dbgroup::atomic::mwcas
{
/**
 * @brief A class to represent an RDCSS target word.
 *
 */
class alignas(kCacheLineSize) MwCASEntry
{
 private:
  /*################################################################################################
   * Internal member variables
   *##############################################################################################*/

  std::atomic<MwCASField> *addr_;

  MwCASField old_;

  MwCASField new_;

  RDCSSDescriptor rdcss_desc_;

 public:
  /*################################################################################################
   * Public constructors/destructors
   *##############################################################################################*/

  template <class T>
  constexpr MwCASEntry(  //
      void *addr,
      const T old_val,
      const T new_val,
      RDCSSDescriptor &&rdcss_desc)
      : addr_{addr}, old_{old_val}, new_{new_val}, rdcss_desc_{rdcss_desc}
  {
  }

  ~MwCASEntry() = default;

  MwCASEntry(const MwCASEntry &) = default;
  MwCASEntry &operator=(const MwCASEntry &obj) = default;
  MwCASEntry(MwCASEntry &&) = default;
  MwCASEntry &operator=(MwCASEntry &&) = default;

  /*################################################################################################
   * Public getters/setters
   *##############################################################################################*/
};

static_assert(sizeof(MwCASEntry) == kCacheLineSize);

}  // namespace dbgroup::atomic::mwcas
