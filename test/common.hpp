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

#ifndef TEST_COMMON_HPP  // NOLINT
#define TEST_COMMON_HPP

#include <functional>

#include "mwcas/utility.hpp"

#ifdef MWCAS_TEST_THREAD_NUM
constexpr size_t kThreadNum = MWCAS_TEST_THREAD_NUM;
#else
constexpr size_t kThreadNum = 8;
#endif

/**
 * @brief An example class to represent CAS-updatable data.
 *
 */
class MyClass
{
 public:
  constexpr MyClass() : data_{}, control_bits_{0} {}
  ~MyClass() = default;

  constexpr MyClass(const MyClass &) = default;
  constexpr MyClass &operator=(const MyClass &) = default;
  constexpr MyClass(MyClass &&) = default;
  constexpr MyClass &operator=(MyClass &&) = default;

  constexpr MyClass &
  operator=(const uint64_t value)
  {
    data_ = value;
    return *this;
  }

  constexpr bool
  operator==(const MyClass &comp) const
  {
    return data_ == comp.data_;
  }

  constexpr bool
  operator!=(const MyClass &comp) const
  {
    return !(*this == comp);
  }

 private:
  uint64_t data_ : 63;
  uint64_t control_bits_ : 1;  // NOLINT
};

namespace dbgroup::atomic::mwcas
{
/**
 * @brief Specialization to enable MwCAS to swap our sample class.
 *
 */
template <>
constexpr bool
CanMwCAS<MyClass>()
{
  return true;
}

}  // namespace dbgroup::atomic::mwcas

#endif  // TEST_COMMON_HPP
