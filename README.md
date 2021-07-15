# MwCAS

![Unit Tests](https://github.com/dbgroup-nagoya-u/mwcas/workflows/Unit%20Tests/badge.svg?branch=main)

This repository is an open source implementation of a multi-word compare-and-swap (MwCAS) operation for research use. This implementation is based on Harris et al.'s CASN operation [1] with some optimizations.

> [1] T. L. Harris, K. Fraser, and I. A. Pratt, "A practical multi-word compare-and-swap operation,” In Proc. DISC, pp. 265-279, 2002.

## Build

**Note**: this is a header only library. You can use this without pre-build.

### Prerequisites

```bash
sudo apt update && sudo apt install -y build-essential cmake
```

### Build Options

- `MWCAS_CAPACITY`: the maximum number of target words of MwCAS: default `4`.

### Build Options for Unit Testing

- `MWCAS_BUILD_TESTS`: build unit tests for MwCAS if `on`: default `off`.
- `MWCAS_TEST_THREAD_NUM`: the number of threads to run unit tests: default `8`.

### Build and Run Unit Tests

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DMWCAS_BUILD_TESTS=on ..
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

### Example Codes

```cpp
#include "mwcas/mwcas_descriptor.hpp"

//...
  // consider double-word CAS with the follwing target fields
  uint64_t word1 = 0, word2 = 0;

//...
  // continue until a MwCAS operation succeeds
  while (true) {
    // prepare a MwCAS descriptor
    dbgroup::atomic::mwcas::MwCASDescriptor desc;

    // read current values of every target word
    const auto old_1 = dbgroup::atomic::mwcas::ReadMwCASField<uint64_t>(&word1);
    const auto old_2 = dbgroup::atomic::mwcas::ReadMwCASField<uint64_t>(&word2);

    // generate desired values
    const auto new_1 = old_1 + 1;
    const auto new_2 = old_2 + 1;

    // add the target words to the descriptor
    desc.AddMwCASTarget(&word1, old_1, new_1);
    desc.AddMwCASTarget(&word2, old_2, new_2);

    // try MwCAS
    if (desc.MwCAS()) break;
  }
//...
```

## Acknowledgments

This work is based on results obtained from project JPNP16007 commissioned by the New Energy and Industrial Technology Development Organization (NEDO). In addition, this work was supported partly by KAKENHI (16H01722 and 20K19804).
