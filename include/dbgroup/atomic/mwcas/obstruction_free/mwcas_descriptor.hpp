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

#ifndef DBGROUP_ATOMIC_MWCAS_OBSTRUCTION_FREE_MWCAS_DESCRIPTOR_HPP_
#define DBGROUP_ATOMIC_MWCAS_OBSTRUCTION_FREE_MWCAS_DESCRIPTOR_HPP_

// C++ standard libraries
#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>

// external C++ libraries
#include <dbgroup/memory/epoch_based_gc.hpp>
#include <dbgroup/memory/utility.hpp>
#include <dbgroup/thread/epoch_guard.hpp>

// local sources
#include "dbgroup/atomic/mwcas/utility.hpp"

namespace dbgroup::atomic::mwcas::obstruction_free
{
/**
 * @brief A class to manage a MwCAS (multi-words compare-and-swap) operation.
 *
 * Readers never help incomplete MwCAS operations forward. Instead, a reader
 * aborts an undecided MwCAS and detaches its descriptor. Since only the owner
 * embeds its descriptor (at most once for each target), duplicated embedded
 * descriptors never exist, and the owner can retire its descriptor by ordinary
 * EBR after detaching all of them.
 *
 * Since readers abort others instead of helping them forward, two MwCAS
 * operations that read each other's targets can abort each other repeatedly.
 * Thus, MwCAS operations are obstruction-free (not lock-free).
 */
class alignas(kCacheLineSize) MwCASDescriptor
{
 public:
  /*##########################################################################*
   * GC settings
   *##########################################################################*/

  /// @brief Do not call destructors.
  using T = void;

  /// @brief Reuse allocated descriptors.
  static constexpr bool kReusePages = true;

  /// @brief The number of retained descriptors in each thread.
  static constexpr size_t kMaxReusableDescriptors = 64;

  /*##########################################################################*
   * Public constructors and assignment operators
   *##########################################################################*/

  /**
   * @brief Construct an empty descriptor for MwCAS operations.
   *
   */
  constexpr MwCASDescriptor() = default;

  MwCASDescriptor(const MwCASDescriptor&) = delete;
  MwCASDescriptor(MwCASDescriptor&&) = delete;

  auto operator=(const MwCASDescriptor& obj) -> MwCASDescriptor& = delete;
  auto operator=(MwCASDescriptor&&) -> MwCASDescriptor& = delete;

  /*##########################################################################*
   * Public destructors
   *##########################################################################*/

  /**
   * @brief Destroy the MwCASDescriptor object.
   *
   */
  ~MwCASDescriptor() = default;

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
   * @brief Start garbage collection for this descriptors.
   *
   * @param gc_interval Interval for GC in microseconds.
   * @param gc_thread_num The number of worker threads to release garbages.
   * @note This function must be called before performing MwCAS.
   */
  static void StartGC(  //
      size_t gc_interval = ::dbgroup::memory::kDefaultGCTime,
      size_t gc_thread_num = ::dbgroup::memory::kDefaultGCThreadNum);

  /**
   * @brief Stop garbage collection for this descriptors.
   *
   */
  static void StopGC();

  /**
   * @return A guard instance for preventing GC.
   */
  static auto CreateEpochGuard()  //
      -> ::dbgroup::thread::EpochGuard;

  /**
   * @return A new descriptor for the MwCAS algorithm.
   * @note You must explicitly delete the given descriptor if you do not call
   * the MwCAS function.
   */
  [[nodiscard]]
  static auto GetDescriptor()  //
      -> MwCASDescriptor*;

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

    targets_.at(target_cnt_++) = MwCASTarget{static_cast<std::atomic_uint64_t*>(addr),
                                             std::bit_cast<uint64_t>(old_val),
                                             std::bit_cast<uint64_t>(new_val), fence};
  }

  /**
   * @brief Perform a MwCAS operation by using registered targets.
   *
   * @retval true if a MwCAS operation succeeds.
   * @retval false otherwise (including the case where a reader has aborted it).
   */
  auto MwCAS()  //
      -> bool;

 private:
  /*##########################################################################*
   * Type aliases
   *##########################################################################*/

  using EpochBasedGC = ::dbgroup::memory::EpochBasedGC<MwCASDescriptor>;

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
  };

  /*##########################################################################*
   * Internal APIs
   *##########################################################################*/

  /**
   * @brief Insert back-off and register a follower if needed.
   *
   * @param[in] addr A target address.
   * @param[in,out] word The current value of a target address.
   * @param[in] fence A memory fence.
   * @retval true if the caller should follow the MwCAS.
   * @retval false otherwise. In this case, `word` is the latest value.
   */
  static auto PrepareToFollow(  //
      std::atomic_uint64_t* addr,
      uint64_t& word,
      std::memory_order fence)  //
      -> bool;

  /**
   * @brief Insert back-off and abort the existing MwCAS if needed.
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
   * @brief Detach this descriptor from all the targets.
   *
   * @param base_addr The address of this descriptor with the flag.
   * @param stat The decided status of this MwCAS.
   * @param embedded_cnt The number of targets embedded by the owner (use zero
   * if the caller is not the owner).
   * @retval true if other threads may refer to this descriptor.
   * @retval false otherwise.
   */
  auto Finish(               //
      uint64_t base_addr,    //
      Status stat,           //
      size_t embedded_cnt)  //
      -> bool;

  /*##########################################################################*
   * Internal member variables
   *##########################################################################*/

  /// @brief The status of this descriptor.
  std::atomic<Status> stat_{kUndecided};

  /// @brief The number of registered MwCAS targets.
  size_t target_cnt_{};

  /// @brief Target entries of MwCAS.
  std::array<MwCASTarget, kMwCASCapacity> targets_ = {};

  /// @brief A garbage collector for expired descriptors.
  static inline std::unique_ptr<EpochBasedGC> _gc{};  // NOLINT
};

}  // namespace dbgroup::atomic::mwcas::obstruction_free

#endif  // DBGROUP_ATOMIC_MWCAS_OBSTRUCTION_FREE_MWCAS_DESCRIPTOR_HPP_
