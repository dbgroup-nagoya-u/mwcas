/*
 * Copyright 2024 Database Group, Nagoya University
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

#ifndef DBGROUP_ATOMIC_MWCAS_LOCK_FREE_AOPT_DESCRIPTOR_HPP_
#define DBGROUP_ATOMIC_MWCAS_LOCK_FREE_AOPT_DESCRIPTOR_HPP_

// C++ standard libraries
#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <utility>

// external libraries
#include "dbgroup/lock/common.hpp"
#include "dbgroup/memory/epoch_based_gc.hpp"
#include "dbgroup/memory/utility.hpp"
#include "dbgroup/thread/epoch_guard.hpp"

// local sources
#include "dbgroup/atomic/mwcas/utility.hpp"

namespace dbgroup::atomic::mwcas::lock_free
{
/**
 * @brief A class for performaing MwCAS with the AOPT algorithm.
 *
 */
class alignas(kCacheLineSize) AOPTDescriptor
{
  /*############################################################################
   * Type aliases
   *##########################################################################*/

  using EpochBasedGC_t = ::dbgroup::memory::EpochBasedGC<AOPTDescriptor>;

 public:
  /*############################################################################
   * GC settings
   *##########################################################################*/

  /// @brief Do not call destructors.
  using T = AOPTDescriptor;

  /// @brief Reuse allocated descriptors.
  static constexpr bool kReusePages = true;

  /// @brief The number of retained descriptors in each thread.
  static constexpr size_t kMaxReusableDescriptors = 64;

  /*############################################################################
   * Public constructors and assignment operators
   *##########################################################################*/

  /**
   * @brief Construct an empty descriptor for MwCAS operations.
   *
   */
  constexpr AOPTDescriptor() = default;

  AOPTDescriptor(const AOPTDescriptor &) = delete;
  AOPTDescriptor(AOPTDescriptor &&) = delete;

  AOPTDescriptor &operator=(const AOPTDescriptor &obj) = delete;
  AOPTDescriptor &operator=(AOPTDescriptor &&) = delete;

  /*############################################################################
   * Public destructors
   *##########################################################################*/

  /**
   * @brief Destroy the AOPTDescriptor object.
   *
   * @note This destructor casts `target_count_` to an atomic variable to
   * prevent compilers from optimizing out it.
   */
  ~AOPTDescriptor()
  {
    std::bit_cast<std::atomic_size_t *>(&target_count_)->store(0, kRelaxed);
    stat_.store(kActive, kRelaxed);
  }

  /*############################################################################
   * Public getters/setters
   *##########################################################################*/

  /**
   * @return the number of registered MwCAS targets.
   */
  [[nodiscard]] constexpr auto
  Size() const  //
      -> size_t
  {
    return target_count_;
  }

  /*############################################################################
   * Public APIs for managing memory
   *##########################################################################*/

  /**
   * @brief Start garbage collection for AOPT descriptors.
   *
   * @param gc_interval Interval for GC in microseconds.
   * @param gc_thread_num The number of worker threads to release garbages.
   * @note This function must be called before performing AOPT-based MwCAS.
   */
  static void StartGC(  //
      size_t gc_interval = ::dbgroup::memory::kDefaultGCTime,
      size_t gc_thread_num = ::dbgroup::memory::kDefaultGCThreadNum);

  /**
   * @brief Stop garbage collection for AOPT descriptors.
   *
   */
  static void StopGC();

  /**
   * @return A guard instance for preventing GC.
   */
  static auto CreateEpochGuard()  //
      -> ::dbgroup::thread::EpochGuard;

  /**
   * @return A new MwCAS descriptor for the AOPT algorithm.
   */
  static auto GetDescriptor()  //
      -> AOPTDescriptor *;

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

    return std::bit_cast<T>(
        ReadInternal(static_cast<const std::atomic_uint64_t *>(addr), nullptr, fence).second);
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

    targets_.at(target_count_++) =
        WordDescriptor{this, static_cast<std::atomic_uint64_t *>(addr), old_val, new_val, fence};
  }

  /**
   * @brief Perform a MwCAS operation by using registered targets.
   *
   * @retval true if a MwCAS operation succeeds.
   * @retval false if a MwCAS operation fails.
   */
  auto MwCAS()  //
      -> bool;

 private:
  /*############################################################################
   * Internal classes
   *##########################################################################*/

  /**
   * @brief A class for representing MwCAS targets.
   *
   */
  struct WordDescriptor {
    /// @brief The address of the corresponding AOPT descriptor.
    AOPTDescriptor *parent;

    /// @brief A target memory address.
    std::atomic_uint64_t *addr;

    /// @brief An expected value of a target field.
    uint64_t old_val;

    /// @brief An inserting value into a target field.
    uint64_t new_val;

    /// @brief A fence to be inserted when embedding a new value.
    std::memory_order fence;
  };

  /**
   * @brief A class for managing completed (i.e., embedded) AOPT descriptors.
   *
   */
  class CompletedDescriptors
  {
   public:
    /*##########################################################################
     * Public constructors and assignment operators
     *########################################################################*/

    /**
     * @brief Create a new CompletedDescriptors object.
     *
     */
    constexpr CompletedDescriptors() = default;

    CompletedDescriptors(const CompletedDescriptors &) = delete;
    CompletedDescriptors(CompletedDescriptors &&) = delete;

    CompletedDescriptors &operator=(const CompletedDescriptors &obj) = delete;
    CompletedDescriptors &operator=(CompletedDescriptors &&) = delete;

    /*##########################################################################
     * Public destructors
     *########################################################################*/

    /**
     * @brief Destroy the CompletedDescriptors object.
     *
     */
    ~CompletedDescriptors();

    /*##########################################################################
     * Public utility functions
     *########################################################################*/

    /**
     * @brief Register a completed descriptor with the internal list.
     *
     * @param desc A completed descriptor.
     * @note If the number of completed descriptors reaches a certain threshold,
     * this function invoke their finalization.
     */
    void RetireForCleanUp(  //
        AOPTDescriptor *desc);

   private:
    /*##########################################################################
     * Internal utility functions
     *########################################################################*/

    /**
     * @brief Perform finalization for AOPT-based descriptors.
     *
     * After this function, the descriptors will be garbage collected.
     */
    void FinalizeCompletedDescriptors();

    /*##########################################################################
     * Internal member variables
     *########################################################################*/

    /// @brief Completed (i.e., embedded) descriptors.
    std::array<AOPTDescriptor *, kMaxReusableDescriptors> desc_arr_ = {};

    /// @brief The current number of completed descriptors.
    size_t desc_num_{};
  };

  /**
   * @brief An enumeration for representing AOPT status.
   *
   */
  enum Status : uint64_t {
    kActive = 0,
    kSuccessful,
    kFailed,
  };

  /*############################################################################
   * Internal utility functions
   *##########################################################################*/

  /**
   * @brief Read a value from a given memory address.
   *
   * @param addr A target memory address to be read.
   * @param self A source descriptor if exist.
   * @param fence A flag for controling std::memory_order.
   * @retval 1st: the actual current word including embedded descriptors.
   * @retval 2nd: the current value.
   */
  static auto ReadInternal(  //
      const std::atomic_uint64_t *addr,
      const AOPTDescriptor *self,
      std::memory_order fence)  //
      -> std::pair<uint64_t, uint64_t>;

  /*############################################################################
   * Internal member variables
   *##########################################################################*/

  /// @brief Target entries of MwCAS.
  std::array<WordDescriptor, kMwCASCapacity> targets_ = {};

  /// @brief The status of this AOPT descriptor.
  std::atomic<Status> stat_{};

  /// @brief The number of registered MwCAS targets.
  size_t target_count_{};

  /// @brief A garbage collector for expired descriptors.
  static inline std::unique_ptr<EpochBasedGC_t> _gc{};  // NOLINT
};

}  // namespace dbgroup::atomic::mwcas::lock_free

#endif  // DBGROUP_ATOMIC_MWCAS_LOCK_FREE_AOPT_DESCRIPTOR_HPP_
