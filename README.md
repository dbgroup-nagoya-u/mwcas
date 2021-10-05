# MwCAS

![Ubuntu-20.04](https://github.com/dbgroup-nagoya-u/mwcas/workflows/Ubuntu-20.04/badge.svg?branch=main)

This repository is an open source implementation of a multi-word compare-and-swap (MwCAS) operation for research use. This implementation is based on Harris et al.'s CASN operation [1] with some optimizations.

> [1] T. L. Harris, K. Fraser, and I. A. Pratt, "A practical multi-word compare-and-swap operation,” In Proc. DISC, pp. 265-279, 2002.

## Build

**Note**: this is a header only library. You can use this without pre-build.

### Prerequisites

```bash
sudo apt update && sudo apt install -y build-essential cmake
```

### Build Options

#### Tuning Parameters

- `MWCAS_CAPACITY`: the maximum number of target words of MwCAS: default `4`.
    - In order to maximize performance, it is desirable to specify the minimum number needed. Otherwise, the extra space will pollute the CPU cache.

#### Parameters for Unit Testing

- `MWCAS_BUILD_TESTS`: build unit tests if `ON`: default `OFF`.
- `MWCAS_TEST_THREAD_NUM`: the number of threads to run unit tests: default `8`.

### Build and Run Unit Tests

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DMWCAS_BUILD_TESTS=ON ..
make -j
ctest -C Release
```

## Usage

### Linking by CMake

1. Download the files in any way you prefer (e.g., `git submodule`).

    ```bash
    cd <your_project_workspace>
    mkdir external
    git submodule add https://github.com/dbgroup-nagoya-u/mwcas.git external/mwcas
    ```

1. Add this library to your build in `CMakeLists.txt`.

    ```cmake
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/mwcas")

    add_executable(
      <target_bin_name>
      [<source> ...]
    )
    target_link_libraries(
      <target_bin_name> PRIVATE
      mwcas
    )
    ```

### MwCAS APIs

The following code shows the basic usage of this library. Note that you need to use a `ReadMwCASField` API to read a current value from a MwCAS target address. Otherwise, you may read an inconsistent data (e.g., an embedded MwCAS descriptor).

```cpp
#include <iostream>
#include <thread>
#include <vector>

#include "mwcas/mwcas_descriptor.hpp"

// use four threads for a multi-threading example
constexpr size_t kThreadNum = 4;

// the number of MwCAS operations in each thread
constexpr size_t kExecNum = 1e6;

// use an unsigned long type as MwCAS targets
using Target = uint64_t;

// aliases for simplicity
using dbgroup::atomic::mwcas::MwCASDescriptor;
using dbgroup::atomic::mwcas::ReadMwCASField;

int
main([[maybe_unused]] int argc, [[maybe_unused]] char** argv)
{
  // targets of a 2wCAS example
  Target word_1 = 0;
  Target word_2 = 0;

  // a lambda function for multi-threading
  auto f = [&]() {
    for (size_t i = 0; i < kExecNum; ++i) {
      // continue until a MwCAS operation succeeds
      while (true) {
        // create a MwCAS descriptor
        MwCASDescriptor desc{};

        // prepare expected/desired values
        const auto old_1 = ReadMwCASField<Target>(&word_1);
        const auto new_1 = old_1 + 1;
        const auto old_2 = ReadMwCASField<Target>(&word_2);
        const auto new_2 = old_2 + 1;

        // register MwCAS targets with the descriptor
        desc.AddMwCASTarget(&word_1, old_1, new_1);
        desc.AddMwCASTarget(&word_2, old_2, new_2);

        // try MwCAS
        if (desc.MwCAS()) break;
      }
    }
  };

  // perform MwCAS operations with multi-threads
  std::vector<std::thread> threads;
  for (size_t i = 0; i < kThreadNum; ++i) threads.emplace_back(f);
  for (auto&& thread : threads) thread.join();

  // check whether MwCAS operations are performed consistently
  std::cout << "1st field: " << word_1 << std::endl  //
            << "2nd field: " << word_2 << std::endl;

  return 0;
}
```

This code will output the following results.

```txt
1st field: 4000000
2nd field: 4000000
```

### Swapping Your Own Classes with MwCAS

By default, this library only deal with `unsigned long` and pointer types as MwCAS targets. To make your own class the target of MwCAS operations, it must satisfy the following conditions:

1. the byte length of the class is `8` (i.e., `static_assert(sizeof(<your_class>) == 8)`),
2. at least the last one bit is reserved for MwCAS and initialized by zeros,
3. the class satisfies [the conditions of the std::atomic template](https://en.cppreference.com/w/cpp/atomic/atomic#Primary_template), and
4. a specialized `CanMwCAS` function is implemented in `dbgroup::atomic::mwcas` namespace and returns `true`.

The following snippet is an example implementation of a class that can be processed by MwCAS operations.

```cpp
struct MyClass {
  /// an actual data
  uint64_t data : 63;

  /// reserve at least one bit for MwCAS operations
  uint64_t control_bits : 1;

  // control bits must be initialzed by zeros
  constexpr MyClass() : data{}, control_bits{0} {
    // ... some initializations ...
  }

  // target class must satisfy conditions of the std::atomic template
  ~MyClass() = default;
  constexpr MyClass(const MyClass &) = default;
  constexpr MyClass &operator=(const MyClass &) = default;
  constexpr MyClass(MyClass &&) = default;
  constexpr MyClass &operator=(MyClass &&) = default;
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
```

## Acknowledgments

This work is based on results obtained from project JPNP16007 commissioned by the New Energy and Industrial Technology Development Organization (NEDO). In addition, this work was supported partly by KAKENHI (16H01722 and 20K19804).
