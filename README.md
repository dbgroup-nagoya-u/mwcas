# MwCAS

![Unit Tests](https://github.com/dbgroup-nagoya-u/mwcas/workflows/Unit%20Tests/badge.svg?branch=main)

An open source implementation of MwCAS (multi-words compare and swap) for research use. This implementation is based on the NCAS (n-words compare and swap) operation [1].

> [1] T. L. Harris, K. Fraser, and I. A. Pratt, “A practical multi-word compare-and-swap operation,” DISC, pp. 265–279, 2002.

## Build

Note: this library is a header only. You can use this as just a submodule.

### Prerequisites

```bash
sudo apt update && sudo apt install -y build-essential cmake
```

### Build Options

- `MWCAS_BUILD_TESTS`: build unit tests for MwCAS if `on`.

### Build and Run Unit Tests

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DMWCAS_BUILD_TESTS=on ..
make -j
ctest -C Release
```
