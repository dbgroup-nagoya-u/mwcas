// Copyright (c) Database Group, Nagoya University. All rights reserved.
// Licensed under the MIT license.

#pragma once

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
static constexpr T
ReadMwCASField(const void *addr)
{
  const auto target_addr = static_cast<const std::atomic<MwCASField> *>(addr);

  MwCASField target_word;
  do {
    target_word = target_addr->load(mo_relax);
  } while (target_word.IsMwCASDescriptor());

  return target_word.GetTargetData<T>();
}

/**
 * @brief A class to manage a MwCAS (multi-words compare-and-swap) operation.
 *
 */
class alignas(kCacheLineSize) MwCASDescriptor
{
 private:
  /*################################################################################################
   * Internal member variables
   *##############################################################################################*/

  /// Target entries of MwCAS
  MwCASTarget targets_[kMwCASCapacity];

  /// The number of registered MwCAS targets
  size_t target_count_;

 public:
  /*################################################################################################
   * Public constructors/destructors
   *##############################################################################################*/

  constexpr MwCASDescriptor() : target_count_{0} {}

  ~MwCASDescriptor() = default;

  constexpr MwCASDescriptor(const MwCASDescriptor &) = default;
  constexpr MwCASDescriptor &operator=(const MwCASDescriptor &obj) = default;
  constexpr MwCASDescriptor(MwCASDescriptor &&) = default;
  constexpr MwCASDescriptor &operator=(MwCASDescriptor &&) = default;

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
  constexpr bool
  MwCAS()
  {
    const auto desc_addr = reinterpret_cast<uintptr_t>(this);
    const auto desc_word = MwCASField{desc_addr, true};

    // serialize MwCAS operations by embedding a descriptor
    auto mwcas_success = true;
    size_t embedded_count = 0;
    for (size_t i = 0; i < target_count_; ++i, ++embedded_count) {
      // embed a MwCAS decriptor
      MwCASField loaded_word;
      do {
        loaded_word = targets_[i].old_val;
        while (!targets_[i].addr->compare_exchange_weak(loaded_word, desc_word, mo_relax)
               && loaded_word == targets_[i].old_val) {
          // weak CAS may fail although it can perform
        }
      } while (loaded_word.IsMwCASDescriptor());

      if (loaded_word != targets_[i].old_val) {
        // if a target filed has been already updated, MwCAS is failed
        mwcas_success = false;
        break;
      }
    }

    // complete MwCAS
    for (size_t i = 0; i < embedded_count; ++i) {
      const auto new_val = (mwcas_success) ? targets_[i].new_val : targets_[i].old_val;
      auto old_val = desc_word;
      while (!targets_[i].addr->compare_exchange_weak(old_val, new_val, mo_relax)
             && old_val == desc_word) {
        // weak CAS may fail although it can perform
      }
    }

    return mwcas_success;
  }
};

}  // namespace dbgroup::atomic::mwcas
