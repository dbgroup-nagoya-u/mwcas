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

#ifndef TEST_COMMON_HPP_
#define TEST_COMMON_HPP_

// C++ standard libraries
#include <cstddef>
#include <cstdint>
#include <functional>

// target library
#include "dbgroup/atomic/mwcas/utility.hpp"

/*##############################################################################
 * Global constants
 *############################################################################*/

constexpr size_t kTestThreadNum = (DBGROUP_TEST_THREAD_NUM);

constexpr size_t kRandomSeed = (DBGROUP_TEST_RANDOM_SEED);

/*##############################################################################
 * Global utility classes
 *############################################################################*/

/**
 * @brief An example class to represent CAS-updatable data.
 *
 */
class MyClass
{
 public:
  constexpr MyClass() = default;
  ~MyClass() = default;

  constexpr MyClass(const MyClass &) = default;
  constexpr MyClass(MyClass &&) noexcept = default;

  constexpr auto operator=(const MyClass &) -> MyClass & = default;
  constexpr auto operator=(MyClass &&) noexcept -> MyClass & = default;

  constexpr auto
  operator=(                 //
      const uint64_t value)  //
      -> MyClass &
  {
    data_ = value;
    return *this;
  }

  constexpr auto
  operator==(                     //
      const MyClass &comp) const  //
      -> bool
  {
    return data_ == comp.data_;
  }

  constexpr auto
  operator!=(                     //
      const MyClass &comp) const  //
      -> bool
  {
    return !(*this == comp);
  }

 private:
  uint64_t data_ : 63 {};
  uint64_t control_bits_ : 1 {};  // NOLINT
};

namespace dbgroup::atomic::mwcas
{
/**
 * @brief Specialization to enable MwCAS to swap our sample class.
 *
 */
template <>
constexpr auto
CanMwCAS<MyClass>()  //
    -> bool
{
  return true;
}

}  // namespace dbgroup::atomic::mwcas

#endif  // TEST_COMMON_HPP_
