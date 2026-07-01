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

#ifndef DBGROUP_ATOMIC_MWCAS_LOCK_FREE_CASN_DESCRIPTOR_HPP_
#define DBGROUP_ATOMIC_MWCAS_LOCK_FREE_CASN_DESCRIPTOR_HPP_

// C++ standard libraries
#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <memory>

// external C++ libraries
#include <dbgroup/lock/utility.hpp>
#include <dbgroup/memory/epoch_based_gc.hpp>
#include <dbgroup/memory/utility.hpp>
#include <dbgroup/thread/epoch_guard.hpp>

// local sources
#include "dbgroup/atomic/mwcas/utility.hpp"

namespace dbgroup::atomic::mwcas::lock_free
{
/**
 * @brief A class for performing MwCAS with the CASN algorithm.
 *
 */
class alignas(kCacheLineSize) CASNDescriptor
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
  constexpr CASNDescriptor() = default;

  CASNDescriptor(const CASNDescriptor&) = delete;
  CASNDescriptor(CASNDescriptor&&) = delete;

  CASNDescriptor& operator=(const CASNDescriptor& obj) = delete;
  CASNDescriptor& operator=(CASNDescriptor&&) = delete;

  /*##########################################################################*
   * Public destructors
   *##########################################################################*/

  /**
   * @brief Destroy the CASNDescriptor object.
   *
   */
  ~CASNDescriptor() = default;

  /*##########################################################################*
   * Public getters/setters
   *##########################################################################*/

  /**
   * @return the number of registered MwCAS targets.
   */
  [[nodiscard]] constexpr auto
  Size() const  //
      -> size_t
  {
    return target_cnt_;
  }

  /*##########################################################################*
   * Public APIs for managing memory
   *##########################################################################*/

  /**
   * @brief Start garbage collection for CASN descriptors.
   *
   * @param gc_interval Interval for GC in microseconds.
   * @param gc_thread_num The number of worker threads to release garbages.
   * @note This function must be called before performing CASN-based MwCAS.
   */
  static void StartGC(  //
      size_t gc_interval = ::dbgroup::memory::kDefaultGCTime,
      size_t gc_thread_num = ::dbgroup::memory::kDefaultGCThreadNum);

  /**
   * @brief Stop garbage collection for CASN descriptors.
   *
   */
  static void StopGC();

  /**
   * @return A guard instance for preventing GC.
   */
  static auto CreateEpochGuard()  //
      -> ::dbgroup::thread::EpochGuard;

  /**
   * @return A new MwCAS descriptor for the CASN algorithm.
   * @note You must explicitly delete the given descriptor if you do not call
   * the MwCAS function.
   */
  [[nodiscard]] static auto GetDescriptor()  //
      -> CASNDescriptor*;

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
      const void* const addr,
      const std::memory_order fence = std::memory_order_seq_cst)  //
      -> T
  {
    static_assert(CanMwCAS<T>());

    const auto* const target_addr = static_cast<const std::atomic_uint64_t*>(addr);
    auto cur = target_addr->load(fence);
    while (true) {
      while (cur & kRDCSSFlag) {
        CompleteRDCSS(cur);
      }
      if ((cur & kMwCASFlag) == 0) break;

      auto* const desc = std::bit_cast<CASNDescriptor*>(cur & kPtrMask);
      desc->MwCASInternal(((cur & kCntMask) >> kCntPos) + 1);
      CPP_UTILITY_SPINLOCK_HINT
      cur = target_addr->load(fence);
    }

    return std::bit_cast<T>(cur);
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

    targets_.at(target_cnt_++) =
        MwCASTarget{static_cast<std::atomic_uint64_t*>(addr), old_val, new_val, fence};
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
   * Type aliases
   *##########################################################################*/

  using EpochBasedGC = ::dbgroup::memory::EpochBasedGC<CASNDescriptor>;

  /*##########################################################################*
   * Internal types
   *##########################################################################*/

  /**
   * @brief An enumeration for representing CASN status.
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
   * Internal constants
   *##########################################################################*/

  /// @brief The second bit from the last indicates RDCSS descriptors.
  static constexpr uint64_t kRDCSSFlag = 1UL << 62UL;

  /// @brief A bit mask for swapping flags with XOR.
  static constexpr uint64_t kFlagSwap = kMwCASFlag | kRDCSSFlag;

  /// @brief The bit position for indicating the original number of a target.
  static constexpr uint64_t kCntPos = 47;

  /// @brief A bit mask for extracting pointers.
  static constexpr uint64_t kPtrMask = (1UL << kCntPos) - 1UL;

  /// @brief A bit mask for extracting the original number of a target.
  static constexpr uint64_t kCntMask = ~kPtrMask ^ (kMwCASFlag | kRDCSSFlag);

  /*##########################################################################*
   * Internal utility functions
   *##########################################################################*/

  /**
   * @brief Complete a found RDCSS operation.
   *
   * @param[in,out] rdcss_addr The address af a target RDCSS descriptor.
   */
  static void CompleteRDCSS(  //
      uint64_t& rdcss_addr);

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
   * @brief Perform a restricted double-compare single-swap operation.
   *
   * @param pos The position of a MwCAS target.
   * @param casn_base The address of a CASN descriptor to be embedded.
   * @return The current value of a target address.
   */
  auto RDCSS(  //
      size_t pos,
      uint64_t casn_base)  //
      -> uint64_t;

  /*##########################################################################*
   * Internal member variables
   *##########################################################################*/

  /// @brief Target entries of MwCAS.
  std::array<MwCASTarget, kMwCASCapacity> targets_ = {};

  /// @brief The status of this CASN descriptor.
  std::atomic<Status> stat_{kUndecided};

  /// @brief The number of registered MwCAS targets.
  size_t target_cnt_{};

  /// @brief A garbage collector for expired descriptors.
  static inline std::unique_ptr<EpochBasedGC> _gc{};  // NOLINT
};

}  // namespace dbgroup::atomic::mwcas::lock_free

#endif  // DBGROUP_ATOMIC_MWCAS_LOCK_FREE_CASN_DESCRIPTOR_HPP_
