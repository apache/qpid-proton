# Fuzz testing for qpid-proton-c

## Dockerfile

Easiest way to build and run the fuzzing is using attached Dockerfile. Run the following command from the top directory of the project

    docker build -f proton-c/src/tests/fuzz/Dockerfile -t qpid-proton-fuzz .
 
Then run the built image and execute a fuzzer to check that all works

    docker run --cap-add SYS_PTRACE -it qpid-proton-fuzz
    ./fuzz-url /src/proton-c/src/tests/fuzz/fuzz-url/crash /src/proton-c/src/tests/fuzz/fuzz-url/corpus
    
You can bind a local directory to the container with the `-v local:remote` option. The `--rm` option is also useful. See https://docs.docker.com/engine/reference/run/.

The docker image is based on `ossfuzz/basebuilder`, which is Ubuntu 16.04 Xenial with clang 5.0 and libc++.

## Building

There are two cmake options to control compilation of fuzzers

* `FUZZ_TEST` (default: `OFF`) adds fuzzers to the build and to regression tests
* `FUZZING_ENGINE` (default: `OFF`) links fuzzers with `libFuzzingEngine` when `ON`

When `FUZZING_ENGINE` is `OFF`, fuzzers are linked with a simple driver suitable only for regression testing.

### with a simple driver

There are no special prerequisites and no extra configuration is necessary.

### with libFuzzer

1. Download and compile libFuzzer. Use http://llvm.org/docs/LibFuzzer.html for detailed instructions.
2. Rename/link `libFuzzer.a` (from previous step) to `libFuzzingEngine.a`
3. Build qpid-proton with the following configuration
  * set `CC` and `CXX` variables to the same compiler you used to build libFuzzer (some recent clang)
  * set `CFLAGS` and `CXXFLAGS` with the coverage and sanitizer(s) you want to use, see libFuzzer documentation for details
  * set `LDFLAGS` to add the directory with `libFuzzingEngine.a` to your link path if necessary
  * set `FUZZ_TEST=ON` and `FUZZING_ENGINE=ON`

For example:

    FLAGS="-fsanitize-coverage=trace-pc-guard -fsanitize=address -fno-omit-frame-pointer -fno-optimize-sibling-calls"
    
    CC=~/third_party/llvm-build/Release+Asserts/bin/clang \
    CXX=~/third_party/llvm-build/Release+Asserts/bin/clang++ \
    CFLAGS="$FLAGS" \
    CXXFLAGS="$FLAGS" \
    LDFLAGS="-L/path/to/libFuzzingEngine/directory" \
    cmake .. -DCMAKE_BUILD_TYPE=Debug -DFUZZ_TEST=ON -DFUZZING_ENGINE=ON

## Running

Execute one of the `fuzz-*` binaries. 

### With simple driver

When given file paths as command line arguments, it will run the fuzzed function with each of the inputs in turn once and then exit.

### WIth libFuzzer

When given a folder as first argument, it will load corpus from the folder and also store newly discovered inputs (that extend code coverage) there. See http://llvm.org/docs/LibFuzzer.html for details.

# Justifications

The reason for renaming the fuzzing library to `libFuzzingEngine` is to be compatible with https://github.com/google/oss-fuzz.

In fuzzing data directories, corpus and crashes are two different subdirectories, because fuzzers can perform corpus minimization. Crashes should not be subjected to corpus minimization, so they need to be kept separately.
