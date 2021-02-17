// Copyright (c) Database Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#pragma once

#include <atomic>

#include "cas_n_field.hpp"
#include "common.hpp"
#include "rdcss_field.hpp"

namespace dbgroup::atomic::mwcas
{
/**
 * @brief A class to represent an RDCSS target word.
 *
 */
class alignas(kWordSize) MwCASField
{
 protected:
  /*################################################################################################
   * Internal member variables
   *##############################################################################################*/

  RDCSSField field_;

 public:
  /*################################################################################################
   * Public constructors/destructors
   *##############################################################################################*/

  constexpr MwCASField() {}

  template <class T>
  constexpr MwCASField(  //
      const T target_data,
      const bool is_mwcas_descriptor = false,
      const bool is_rdcss_descriptor = false)
      : field_{RDCSSField{CASNField{target_data, is_mwcas_descriptor}, is_rdcss_descriptor}}
  {
  }

  constexpr MwCASField(const RDCSSField &&rdcss_field) : field_{rdcss_field} {}

  ~MwCASField() = default;

  MwCASField(const MwCASField &) = default;
  MwCASField &operator=(const MwCASField &obj) = default;
  MwCASField(MwCASField &&) = default;
  MwCASField &operator=(MwCASField &&) = default;

  constexpr bool
  operator==(const MwCASField &obj) const
  {
    return this->field_ == obj.field_;
  }

  constexpr bool
  operator!=(const MwCASField &obj) const
  {
    return this->field_ != obj.field_;
  }

  /*################################################################################################
   * Public getters/setters
   *##############################################################################################*/

  constexpr bool
  IsMwCASDescriptor() const
  {
    return field_.GetTargetData<CASNField>().IsMwCASDescriptor();
  }

  constexpr bool
  IsRDCSSDescriptor() const
  {
    return field_.IsRDCSSDescriptor();
  }

  template <class T>
  constexpr T
  GetTargetData() const
  {
    return field_.GetTargetData<CASNField>().GetTargetData<T>();
  }
};

// CAS target words must be one word
static_assert(sizeof(MwCASField) == kWordSize);

}  // namespace dbgroup::atomic::mwcas
