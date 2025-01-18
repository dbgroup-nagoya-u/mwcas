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

#ifndef DBGROUP_ATOMIC_MWCAS_DEADLOCK_FREE_MWCAS_DESCRIPTOR_HPP_
#define DBGROUP_ATOMIC_MWCAS_DEADLOCK_FREE_MWCAS_DESCRIPTOR_HPP_

// C++ standard libraries
#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <thread>

// external libraries
#include "dbgroup/lock/common.hpp"

// local sources
#include "dbgroup/atomic/mwcas/utility.hpp"

namespace dbgroup::atomic::mwcas::lock_free
{
/**
 * @brief A class to manage a MwCAS (multi-words compare-and-swap) operation.
 *
 */
class alignas(kCacheLineSize) MwCASDescriptor
{
 public:
  /*############################################################################
   * Public constructors and assignment operators
   *##########################################################################*/

  /**
   * @brief Construct an empty descriptor for MwCAS operations.
   *
   */
  constexpr MwCASDescriptor() = default;

  constexpr MwCASDescriptor(const MwCASDescriptor &) = default;
  constexpr MwCASDescriptor(MwCASDescriptor &&) noexcept = default;

  constexpr auto operator=(const MwCASDescriptor &obj) -> MwCASDescriptor & = default;
  constexpr auto operator=(MwCASDescriptor &&) noexcept -> MwCASDescriptor & = default;

  /*############################################################################
   * Public destructors
   *##########################################################################*/

  /**
   * @brief Destroy the MwCASDescriptor object.
   *
   */
  ~MwCASDescriptor() = default;

  /*############################################################################
   * Public getters/setters
   *##########################################################################*/

  /**
   * @return The number of registered MwCAS targets.
   */
  [[nodiscard]] constexpr auto
  Size() const  //
      -> size_t
  {
    return target_cnt_;
  }

  /*############################################################################
   * Public utility functions
   *##########################################################################*/

  /**
   * @brief Read a value from a given memory address.
   *
   * @tparam T An expected class of a target field.
   * @param addr A target memory address to read.
   * @param fence A flag for controling std::memory_order.
   * @return A read value.
   * @note If a memory address is included in MwCAS target fields, it must be
   * read via this function.
   */
  template <class T>
  static auto
  Read(  //
      const void *addr,
      const std::memory_order fence = std::memory_order_seq_cst)  //
      -> T
  {
    static_assert(CanMwCAS<T>());

    const auto *target_addr = static_cast<const std::atomic_uint64_t *>(addr);
    uint64_t word{};
    while (true) {
      for (size_t i = 1; true; ++i) {
        word = target_addr->load(fence);
        if ((word & kMwCASFlag) == 0) return std::bit_cast<T>(word);
        if (i > kRetryNum) break;
        CPP_UTILITY_SPINLOCK_HINT
      }
      std::this_thread::sleep_for(kBackOffTime);
    }
  }

  /**
   * @brief Add a new MwCAS target to this descriptor.
   *
   * @tparam T The class of a target word.
   * @param addr A target memory address.
   * @param old_val The expected value of a target field.
   * @param new_val An inserting value into a target field.
   * @param fence A flag for controling std::memory_order.
   */
  template <class T>
  constexpr void
  AddMwCASTarget(  //
      void *addr,
      const T old_val,
      const T new_val,
      const std::memory_order fence = std::memory_order_seq_cst)
  {
    static_assert(CanMwCAS<T>());

    targets_.at(target_cnt_++) =
        MwCASTarget{static_cast<std::atomic_uint64_t *>(addr), old_val, new_val, fence};
  }

  /**
   * @brief Perform a MwCAS operation by using registered targets.
   *
   * @retval true if a MwCAS operation succeeds.
   * @retval false otherwise.
   */
  auto MwCAS()  //
      -> bool;

 private:
  /*############################################################################
   * Internal types
   *##########################################################################*/

  /**
   * @brief An enumeration for representing MwCAS status.
   *
   */
  enum Status : uint64_t {
    kUndecided = 0,
    kSucceeded,
    kFailed,
  };

  /**
   * @brief A class for representing MwCAS targets.
   *
   */
  struct MwCASTarget {
    /// @brief A target memory address.
    std::atomic_uint64_t *addr;

    /// @brief An expected value of a target field.
    uint64_t old_val;

    /// @brief An inserting value into a target field.
    uint64_t new_val;

    /// @brief A fence to be inserted when embedding a new value.
    std::memory_order fence;
  };

  /*############################################################################
   * Internal APIs
   *##########################################################################*/

  /**
   * @brief Embed a descriptor into this target address to linearlize MwCAS.
   *
   * @param desc_addr The address of this descriptor with a MwCAS flag.
   * @param pos The position of a target word.
   * @retval true if the descriptor address is successfully embedded.
   * @retval false otherwise.
   */

  /*############################################################################
   * Internal member variables
   *##########################################################################*/

  /// @brief The status of this CASN descriptor.
  std::atomic<Status> stat_{kUndecided};

  /// @brief Target entries of MwCAS.
  std::array<MwCASTarget, kMwCASCapacity> targets_ = {};

  /// @brief The number of registered MwCAS targets.
  size_t target_cnt_{};
};

}  // namespace dbgroup::atomic::mwcas::lock_free

#endif  // DBGROUP_ATOMIC_MWCAS_DEADLOCK_FREE_MWCAS_DESCRIPTOR_HPP_
