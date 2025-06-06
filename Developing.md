# Getting Started with Developing CUDA-Q

This document contains guidelines for contributing to the code in this
repository. This document is relevant primarily for contributions to the `nvq++`
compiler, the CUDA-Q runtime, or to the integrated simulation backends. If you
would like to contribute applications and examples that use the CUDA-Q platform,
please follow the instructions for [installing CUDA-Q][official_install]
instead.

[official_install]: https://nvidia.github.io/cuda-quantum/latest/using/quick_start.html#install-cuda-q

## Quick start guide

Before getting started with development, please create a fork of this repository
if you haven't done so already and make sure to check out the latest version on
the `main` branch. After following the instruction for [setting up your
development environment](./Dev_Setup.md) and [building CUDA-Q from
source](Building.md), you should be able to confirm that you can run the tests
and examples using your local build. If you edit [this
file](./runtime/nvqir/CircuitSimulator.h) to add a print statement

```c++
std::cout << "Custom registration of " << #PRINTED_NAME << "\n" << std::endl;
```

to the definition of the `NVQIR_REGISTER_SIMULATOR` macro, you should see this
line printed when you build the code and run an example using the command

```bash
bash "$CUDAQ_REPO_ROOT/scripts/build_cudaq.sh" && \
nvq++ "$CUDAQ_REPO_ROOT/docs/sphinx/applications/cpp/grover.cpp" -o grover.out && \
./grover.out
# Build with specific options - verbose, debug build, run 8 jobs in parallel
bash "$CUDAQ_REPO_ROOT/scripts/build_cudaq.sh" -v -c Debug -j 8
```

Incremental Rebuilds
After making changes, you can rebuild specific components without rebuilding everything:

```bash
cd "$CUDAQ_REPO_ROOT/build"
# Rebuild - faster than full build from scratch
ninja
# Rebuild specific component
ninja <target>
```

When working on compiler internals, it can be useful to look at intermediate
representations for CUDA-Q kernels.

To see how the kernels in [this
example](./docs/sphinx/applications/cpp/grover.cpp) are translated, you
can run

```bash
cudaq-quake $CUDAQ_REPO_ROOT/docs/sphinx/applications/cpp/grover.cpp
```

to see its representation in the Quake MLIR dialect. To see its translation to
[QIR](https://www.qir-alliance.org/), you can run

```bash
cudaq-quake $CUDAQ_REPO_ROOT/docs/sphinx/applications/cpp/grover.cpp |
cudaq-opt --canonicalize --add-dealloc |
quake-translate --convert-to=qir
```

## Code style

With regards to code format and style, we distinguish public APIs and CUDA-Q
internals. Public APIs should follow the style guide of the respective language,
specifically [this guide][cpp_style] for C++, and the [this guide][python_style]
for Python. The CUDA-Q internals on the other hand follow the [MLIR/LLVM style
guide][llvm_style]. Please ensure that your code includes comprehensive doc
comments as well as a comment at the top of the file to indicating its purpose.

[python_style]: https://google.github.io/styleguide/pyguide.html
[cpp_style]: https://www.gnu.org/prep/standards/standards.html
[llvm_style]: https://llvm.org/docs/CodingStandards.html

### C++ Formatting

```bash
bash "$CUDAQ_REPO_ROOT/scripts/run_clang_format.sh"
```

### Python Formatting

```bash
yapf -i <file_name>.py
# To run recursively on a directory
yapf -i --recursive <directory>
```

### Spell checker

```bash
bash "$CUDAQ_REPO_ROOT/scripts/run_all_spelling.sh"
# To only check the files that have changed from `main`
bash "$CUDAQ_REPO_ROOT/scripts/run_all_spelling.sh" -d
```

## Testing and debugging

CUDA-Q tests are categorized as unit tests on runtime library code and
`FileCheck` tests on compiler code output. All code added under the runtime
libraries should have an accompanying test added to the appropriate spot in the
`unittests` folder. All code that directly impacts compiler code should have an
accompanying `FileCheck` test. These tests are located in the `test` folder.

### C++ tests

```bash
cd "$CUDAQ_REPO_ROOT/build"
ctest
# To run a specific test
ctest -R <test-name>
```

### Python tests

```bash
python3 -m pytest -v python/tests/ --ignore python/tests/backends
for backendTest in python/tests/backends/*.py; do python3 -m pytest -v $backendTest; done
```

When running a CUDA-Q executable locally, the verbosity of the output can be
configured by setting the `CUDAQ_LOG_LEVEL` environment variable. Setting its
value to `info` will enable printing of informational messages, and setting its
value to `trace` generates additional messages to trace the execution. These
logs can be directed to a file by setting the `CUDAQ_LOG_FILE` variable when
invoking the executable, e.g.

```bash
CUDAQ_LOG_FILE=grover_log.txt CUDAQ_LOG_LEVEL=info grover.out
```
