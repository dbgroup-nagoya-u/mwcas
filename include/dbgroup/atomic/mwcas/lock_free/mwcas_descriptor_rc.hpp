/*
 * Copyright 2025 Database Group, Nagoya University
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

#ifndef DBGROUP_ATOMIC_MWCAS_LOCK_FREE_MWCAS_DESCRIPTOR_RC_HPP_
#define DBGROUP_ATOMIC_MWCAS_LOCK_FREE_MWCAS_DESCRIPTOR_RC_HPP_

// C++ standard libraries
#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>

// local sources
#include "dbgroup/atomic/mwcas/utility.hpp"

namespace dbgroup::atomic::mwcas::lock_free
{
/**
 * @brief A class to manage a MwCAS (multi-words compare-and-swap) operation.
 *
 * This class reclaims descriptors by split reference counting instead of EBR.
 * - The external count is the counter embedded in each target word. A follower
 *   acquires a reference by incrementing it with CAS, so validating that the
 *   descriptor is still embedded and acquiring a reference are a single CAS.
 * - The internal count (`ref_cnt_`) is the number of references that have been
 *   transferred from target words, plus the owner's reference.
 * - A thread that detaches an embedded descriptor adds the external count of the
 *   detached word to the internal count *before* its CAS (and subtracts it if
 *   the CAS fails), so that the transfer is visible when the detachment is.
 * - Every thread releases its reference only after the word it followed has been
 *   detached. Thus, a released reference has always been transferred.
 *
 * Callers do not need epoch guards for descriptors.
 */
class alignas(kCacheLineSize) MwCASDescriptorRC
{
 public:
  /*##########################################################################*
   * Public constants
   *##########################################################################*/

  /// @brief The number of retained descriptors in each thread.
  static constexpr size_t kMaxReusableDescriptors = 64;

  /*##########################################################################*
   * Public constructors and assignment operators
   *##########################################################################*/

  /**
   * @brief Construct an empty descriptor for MwCAS operations.
   *
   */
  constexpr MwCASDescriptorRC() = default;

  MwCASDescriptorRC(const MwCASDescriptorRC&) = delete;
  MwCASDescriptorRC(MwCASDescriptorRC&&) = delete;

  auto operator=(const MwCASDescriptorRC& obj) -> MwCASDescriptorRC& = delete;
  auto operator=(MwCASDescriptorRC&&) -> MwCASDescriptorRC& = delete;

  /*##########################################################################*
   * Public destructors
   *##########################################################################*/

  /**
   * @brief Destroy the MwCASDescriptorRC object.
   *
   */
  ~MwCASDescriptorRC() = default;

  /*##########################################################################*
   * Public getters/setters
   *##########################################################################*/

  /**
   * @return The number of registered MwCAS targets.
   */
  [[nodiscard]]
  constexpr auto
  Size() const  //
      -> size_t
  {
    return target_cnt_;
  }

  /*##########################################################################*
   * Public APIs for managing memory
   *##########################################################################*/

  /**
   * @return A new descriptor for the MwCAS algorithm.
   * @note You must explicitly delete the given descriptor if you do not call
   * the MwCAS function.
   */
  [[nodiscard]]
  static auto GetDescriptor()  //
      -> MwCASDescriptorRC*;

  /*##########################################################################*
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
      void* const addr,
      const std::memory_order fence = std::memory_order_seq_cst)  //
      -> T
  {
    static_assert(CanMwCAS<T>());

    auto* const target_addr = static_cast<std::atomic_uint64_t*>(addr);
    auto word = target_addr->load(fence);
    while (word & kMwCASFlag) {
      FollowIfNeeded(target_addr, word, fence);
    }
    return std::bit_cast<T>(word);
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
      void* const addr,
      const T old_val,
      const T new_val,
      const std::memory_order fence = std::memory_order_seq_cst)
  {
    static_assert(CanMwCAS<T>());

    new (&(targets_.at(target_cnt_++))) MwCASTarget{static_cast<std::atomic_uint64_t*>(addr),
                                                    old_val, new_val, fence, kInitialThreadId};
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
  /*##########################################################################*
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
    std::atomic_uint64_t* addr;

    /// @brief An expected value of a target field.
    uint64_t old_val;

    /// @brief An inserting value into a target field.
    uint64_t new_val;

    /// @brief A fence to be inserted when embedding a new value.
    std::memory_order fence;

    /// @brief A thread ID to be compared for verification.
    std::atomic_size_t thread_id;
  };

  /*##########################################################################*
   * Internal constants
   *##########################################################################*/

  /// @brief An initial thread ID assigned before the target is processed.
  static constexpr size_t kInitialThreadId = ~0UL;

  /*##########################################################################*
   * Internal APIs
   *##########################################################################*/

  /**
   * @brief Insert back-off and acquire a reference by incrementing the counter.
   *
   * @param[in] addr A target address.
   * @param[in,out] word The current value of a target address.
   * @param[in] fence A memory fence.
   * @retval true if the caller has acquired a reference. In this case, `word`
   * is the incremented value.
   * @retval false otherwise. In this case, `word` is the latest value.
   */
  static auto PrepareToFollow(  //
      std::atomic_uint64_t* addr,
      uint64_t& word,
      std::memory_order fence)  //
      -> bool;

  /**
   * @brief Insert back-off and follow the existing MwCAS if needed.
   *
   * @param[in] addr A target address.
   * @param[in,out] word The current value of a target address.
   * @param[in] fence A memory fence.
   */
  static void FollowIfNeeded(  //
      std::atomic_uint64_t* addr,
      uint64_t& word,
      std::memory_order fence);

  /**
   * @brief Recycle a descriptor that no thread refers to.
   *
   * @param desc A descriptor to be recycled.
   */
  static void Recycle(  //
      MwCASDescriptorRC* desc);

  /**
   * @brief Release a reference and recycle this descriptor if it is the last one.
   *
   */
  void Release();

  /**
   * @brief Try to detach this descriptor from a target word.
   *
   * The external count in `expected` is transferred to the internal count
   * before the CAS, and the transfer is canceled if the CAS fails.
   *
   * @param[in] addr A target address.
   * @param[in,out] expected An expected word. It is updated if the CAS fails.
   * @param[in] desired A value to be written.
   * @retval true if this descriptor has been detached.
   * @retval false otherwise.
   */
  auto TryDetach(  //
      std::atomic_uint64_t* addr,
      uint64_t& expected,
      uint64_t desired)  //
      -> bool;

  /**
   * @brief Swap all the embedded descriptors in a target into a old value.
   *
   * @param desc_addr The address of this descriptor with the flag.
   * @param target A target MwCAS information.
   */
  void Abort(              //
      uint64_t desc_addr,  //
      MwCASTarget& target);

  /**
   * @brief Swap the agreed embedded descriptor in a target into a new value and
   * duplicated ones into a old value.
   *
   * @param desc_addr The address of this descriptor with the flag.
   * @param target A target MwCAS information.
   */
  void Complete(           //
      uint64_t desc_addr,  //
      MwCASTarget& target);

  /**
   * @brief An actual MwCAS procedure.
   *
   * @param begin_pos The begin position of target words.
   * @retval true if a MwCAS operation succeeds.
   * @retval false otherwise.
   */
  auto MwCASInternal(        //
      size_t begin_pos = 0)  //
      -> bool;

  /**
   * @brief Determine and verify the thread ID of a target and clean up if it's a duplicated
   * embedded descriptor.
   *
   * @param pos The index of a target.
   * @param word The value read from the target address.
   * @retval true if the finalized thread ID matches the embedded thread ID.
   * @retval false otherwise.
   */
  auto DetermineThreadId(  //
      size_t pos,          //
      uint64_t word)       //
      -> bool;

  /*##########################################################################*
   * Internal member variables
   *##########################################################################*/

  /// @brief The status of this descriptor.
  std::atomic<Status> stat_{kUndecided};

  /// @brief The internal reference count (including the owner's reference).
  std::atomic_int64_t ref_cnt_{1};

  /// @brief The number of registered MwCAS targets.
  size_t target_cnt_{};

  /// @brief Target entries of MwCAS.
  std::array<MwCASTarget, kMwCASCapacity> targets_ = {};
};

}  // namespace dbgroup::atomic::mwcas::lock_free

#endif  // DBGROUP_ATOMIC_MWCAS_LOCK_FREE_MWCAS_DESCRIPTOR_RC_HPP_
