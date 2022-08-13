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

#ifndef MWCAS_COMPONENT_MWCAS_TARGET_HPP
#define MWCAS_COMPONENT_MWCAS_TARGET_HPP

#include <atomic>

#include "mwcas_field.hpp"

namespace dbgroup::atomic::mwcas::component
{
/**
 * @brief A class to represent a MwCAS target.
 *
 */
class MwCASTarget
{
 public:
  /*####################################################################################
   * Public constructors and assignment operators
   *##################################################################################*/

  /**
   * @brief Construct an empty MwCAS target.
   *
   */
  constexpr MwCASTarget() = default;

  /**
   * @brief Construct a new MwCAS target based on given information.
   *
   * @tparam T a class of MwCAS targets.
   * @param addr a target memory address.
   * @param old_val an expected value of the target address.
   * @param new_val an desired value of the target address.
   */
  template <class T>
  constexpr MwCASTarget(  //
      void *addr,
      const T old_val,
      const T new_val)
      : addr_{static_cast<std::atomic<MwCASField> *>(addr)}, old_val_{old_val}, new_val_{new_val}
  {
  }

  constexpr MwCASTarget(const MwCASTarget &) = default;
  constexpr auto operator=(const MwCASTarget &obj) -> MwCASTarget & = default;
  constexpr MwCASTarget(MwCASTarget &&) = default;
  constexpr auto operator=(MwCASTarget &&) -> MwCASTarget & = default;

  /*####################################################################################
   * Public destructor
   *##################################################################################*/

  /**
   * @brief Destroy the MwCASTarget object.
   *
   */
  ~MwCASTarget() = default;

  /*####################################################################################
   * Public utility functions
   *##################################################################################*/

  /**
   * @brief Embed a descriptor into this target address to linearlize MwCAS operations.
   *
   * @param desc_addr a memory address of a target descriptor.
   * @retval true if the descriptor address is successfully embedded.
   * @retval false otherwise.
   */
  auto
  EmbedDescriptor(const MwCASField desc_addr)  //
      -> bool
  {
    MwCASField expected = old_val_;
    for (size_t i = 1; true; ++i) {
      // try to embed a MwCAS decriptor
      addr_->compare_exchange_strong(expected, desc_addr, std::memory_order_relaxed);
      if (!expected.IsMwCASDescriptor() || i >= kRetryNum) break;

      // retry if another desctiptor is embedded
      expected = old_val_;
      MWCAS_SPINLOCK_HINT
    }

    return expected == old_val_;
  }

  /**
   * @brief Update/revert a value of this target address.
   *
   * @param desc_addr an embedded descriptor in this target address.
   * @param mwcas_success a flag to indicate a target will be updated or reverted.
   */
  void
  CompleteMwCAS(const bool mwcas_success)
  {
    const MwCASField desired = (mwcas_success) ? new_val_ : old_val_;
    addr_->store(desired, std::memory_order_relaxed);
  }

 private:
  /*####################################################################################
   * Internal constants
   *##################################################################################*/

  /// the maximum number of retries for preventing busy loops.
  static constexpr size_t kRetryNum = 10UL;

  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

  /// A target memory address
  std::atomic<MwCASField> *addr_{};

  /// An expected value of a target field
  MwCASField old_val_{};

  /// An inserting value into a target field
  MwCASField new_val_{};
};

}  // namespace dbgroup::atomic::mwcas::component

#endif  // MWCAS_COMPONENT_MWCAS_TARGET_HPP
