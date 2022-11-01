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

#ifndef MWCAS_MWCAS_DESCRIPTOR_HPP
#define MWCAS_MWCAS_DESCRIPTOR_HPP

#include <array>
#include <atomic>
#include <chrono>
#include <thread>
#include <utility>

#include "component/mwcas_target.hpp"

namespace dbgroup::atomic::mwcas
{
/**
 * @brief A class to manage a MwCAS (multi-words compare-and-swap) operation.
 *
 */
class alignas(component::kCacheLineSize) MwCASDescriptor
{
  /*####################################################################################
   * Type aliases
   *##################################################################################*/

  using MwCASTarget = component::MwCASTarget;
  using MwCASField = component::MwCASField;

 public:
  /*####################################################################################
   * Public constructors and assignment operators
   *##################################################################################*/

  /**
   * @brief Construct an empty descriptor for MwCAS operations.
   *
   */
  constexpr MwCASDescriptor() = default;

  constexpr MwCASDescriptor(const MwCASDescriptor &) = default;
  constexpr auto operator=(const MwCASDescriptor &obj) -> MwCASDescriptor & = default;
  constexpr MwCASDescriptor(MwCASDescriptor &&) = default;
  constexpr auto operator=(MwCASDescriptor &&) -> MwCASDescriptor & = default;

  /*####################################################################################
   * Public destructors
   *##################################################################################*/

  /**
   * @brief Destroy the MwCASDescriptor object.
   *
   */
  ~MwCASDescriptor() = default;

  /*####################################################################################
   * Public getters/setters
   *##################################################################################*/

  /**
   * @return the number of registered MwCAS targets
   */
  [[nodiscard]] constexpr auto
  Size() const  //
      -> size_t
  {
    return target_count_;
  }

  /*####################################################################################
   * Public utility functions
   *##################################################################################*/

  /**
   * @brief Read a value from a given memory address.
   * \e NOTE: if a memory address is included in MwCAS target fields, it must be read
   * via this function.
   *
   * @tparam T an expected class of a target field
   * @param addr a target memory address to read
   * @param fence a flag for controling std::memory_order.
   * @return a read value
   */
  template <class T>
  static auto
  Read(  //
      const void *addr,
      const std::memory_order fence = std::memory_order_seq_cst)  //
      -> T
  {
    const auto *target_addr = static_cast<const std::atomic<MwCASField> *>(addr);

    MwCASField target_word{};
    while (true) {
      for (size_t i = 1; true; ++i) {
        target_word = target_addr->load(fence);
        if (!target_word.IsMwCASDescriptor()) return target_word.GetTargetData<T>();
        if (i > kRetryNum) break;
        MWCAS_SPINLOCK_HINT
      }

      // wait to prevent busy loop
      std::this_thread::sleep_for(kShortSleep);
    }
  }

  /**
   * @brief Add a new MwCAS target to this descriptor.
   *
   * @tparam T a class of a target
   * @param addr a target memory address
   * @param old_val an expected value of a target field
   * @param new_val an inserting value into a target field
   * @param fence a flag for controling std::memory_order.
   */
  template <class T>
  constexpr void
  AddMwCASTarget(  //
      void *addr,
      const T old_val,
      const T new_val,
      const std::memory_order fence = std::memory_order_seq_cst)
  {
    assert(target_count_ < kMwCASCapacity);

    targets_[target_count_++] = MwCASTarget{addr, old_val, new_val, fence};
  }

  /**
   * @brief Perform a MwCAS operation by using registered targets.
   *
   * @retval true if a MwCAS operation succeeds
   * @retval false if a MwCAS operation fails
   */
  auto
  MwCAS()  //
      -> bool
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
    if (mwcas_success) {
      for (size_t i = 0; i < embedded_count; ++i) {
        targets_[i].RedoMwCAS();
      }
    } else {
      for (size_t i = 0; i < embedded_count; ++i) {
        targets_[i].UndoMwCAS();
      }
    }

    return mwcas_success;
  }

 private:
  /*####################################################################################
   * Internal member variables
   *##################################################################################*/

  /// Target entries of MwCAS
  MwCASTarget targets_[kMwCASCapacity];

  /// The number of registered MwCAS targets
  size_t target_count_{0};
};

}  // namespace dbgroup::atomic::mwcas

#endif  // MWCAS_MWCAS_DESCRIPTOR_HPP
