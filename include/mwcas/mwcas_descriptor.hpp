/*
 * Copyright 2021 Database Group, Nagoya University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MWCAS_MWCAS_MWCAS_DESCRIPTOR_H_
#define MWCAS_MWCAS_MWCAS_DESCRIPTOR_H_

#include <array>
#include <atomic>
#include <utility>

#include "components/mwcas_target.hpp"

namespace dbgroup::atomic::mwcas
{
/**
 * @brief Read a value from a given memory address.
 * \e NOTE: if a memory address is included in MwCAS target fields, it must be read via
 * this function.
 *
 * @tparam T an expected class of a target field
 * @param addr a target memory address to read
 * @return a read value
 */
template <class T>
T
ReadMwCASField(const void *addr)
{
  const auto target_addr = static_cast<const std::atomic<component::MwCASField> *>(addr);

  component::MwCASField target_word;
  do {
    target_word = target_addr->load(component::mo_relax);
  } while (target_word.IsMwCASDescriptor());

  return target_word.GetTargetData<T>();
}

/**
 * @brief A class to manage a MwCAS (multi-words compare-and-swap) operation.
 *
 */
class alignas(component::kCacheLineSize) MwCASDescriptor
{
 public:
  /*################################################################################################
   * Public constructors and assignment operators
   *##############################################################################################*/

  /**
   * @brief Construct an empty descriptor for MwCAS operations.
   *
   */
  constexpr MwCASDescriptor() : target_count_{0} {}

  constexpr MwCASDescriptor(const MwCASDescriptor &) = default;
  constexpr MwCASDescriptor &operator=(const MwCASDescriptor &obj) = default;
  constexpr MwCASDescriptor(MwCASDescriptor &&) = default;
  constexpr MwCASDescriptor &operator=(MwCASDescriptor &&) = default;

  /*################################################################################################
   * Public destructors
   *##############################################################################################*/

  /**
   * @brief Destroy the MwCASDescriptor object.
   *
   */
  ~MwCASDescriptor() = default;

  /*################################################################################################
   * Public getters/setters
   *##############################################################################################*/

  /**
   * @return the number of registered MwCAS targets
   */
  constexpr size_t
  Size() const
  {
    return target_count_;
  }

  /*################################################################################################
   * Public utility functions
   *##############################################################################################*/

  /**
   * @brief Add a new MwCAS target to this descriptor.
   *
   * @tparam T a class of a target
   * @param addr a target memory address
   * @param old_val an expected value of a target field
   * @param new_val an inserting value into a target field
   * @retval true if target registration succeeds
   * @retval false if this descriptor is already full
   */
  template <class T>
  constexpr bool
  AddMwCASTarget(  //
      void *addr,
      const T old_val,
      const T new_val)
  {
    if (target_count_ == kMwCASCapacity) {
      return false;
    } else {
      targets_[target_count_++] = MwCASTarget{addr, old_val, new_val};
      return true;
    }
  }

  /**
   * @brief Perform a MwCAS operation by using registered targets.
   *
   * @retval true if a MwCAS operation succeeds
   * @retval false if a MwCAS operation fails
   */
  bool
  MwCAS()
  {
    const MwCASField desc_addr{this, true};

    // serialize MwCAS operations by embedding a descriptor
    auto mwcas_success = true;
    size_t embedded_count = 0;
    for (size_t i = 0; i < target_count_; ++i, ++embedded_count) {
      if (!targets_[i].EmbedDescriptor(desc_addr)) {
        // if a target field has been already updated, MwCAS fails
        mwcas_success = false;
        break;
      }
    }

    // complete MwCAS
    for (size_t i = 0; i < embedded_count; ++i) {
      targets_[i].CompleteMwCAS(desc_addr, mwcas_success);
    }

    return mwcas_success;
  }

 private:
  /*################################################################################################
   * Internal type aliases
   *##############################################################################################*/

  using MwCASTarget = component::MwCASTarget;
  using MwCASField = component::MwCASField;

  /*################################################################################################
   * Internal member variables
   *##############################################################################################*/

  /// Target entries of MwCAS
  MwCASTarget targets_[kMwCASCapacity];

  /// The number of registered MwCAS targets
  size_t target_count_;
};

}  // namespace dbgroup::atomic::mwcas

#endif  // MWCAS_MWCAS_MWCAS_DESCRIPTOR_H_
