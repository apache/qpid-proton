# Microbenchmarks

## Compiling

    -DENABLE_BENCHMARKS

## Running

Suppress CPU frequency scaling

    # https://www.kernel.org/doc/html/latest/admin-guide/pm/intel_pstate.html
    echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo

Maybe reserve specific cores for the benchmark and and possibly also switch off Multi Threading (on Intel).
This makes the results less noisy.

    # https://llvm.org/docs/Benchmarking.html

Let Google Benchmark compute average, median, stddev of time from multiple executions

    --benchmark_repetitions=9

## Profiling

    sudo sh -c 'echo 1 > /proc/sys/kernel/perf_event_paranoid'
    sudo sh -c 'echo 0 > /proc/sys/kernel/kptr_restrict'
