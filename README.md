# MwCAS

![Unit Tests](https://github.com/dbgroup-nagoya-u/mwcas/workflows/Unit%20Tests/badge.svg?branch=main)

An open source implementation of MwCAS (multi-words compare and swap) for research use. This implementation is based on the NCAS (n-words compare and swap) operation [1].

> [1] T. L. Harris, K. Fraser, and I. A. Pratt, “A practical multi-word compare-and-swap operation,” DISC, pp. 265–279, 2002.

## Build

**Note**: this is a header only library. You can use this without pre-build.

### Prerequisites

```bash
sudo apt update && sudo apt install -y build-essential cmake
```

### Build Options

- `MWCAS_CAPACITY`: the number of maximum target fields of MwCAS: default `4`.
- `MWCAS_BUILD_TESTS`: build unit tests for MwCAS if `on`: default `off`.
- `MWCAS_TEST_THREAD_NUM`: the number of threads to run unit tests: default `8`.

### Build and Run Unit Tests

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DMWCAS_BUILD_TESTS=on ..
make -j
ctest -C Release
```
