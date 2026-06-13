# benchmarks/

Programs that **measure speed**, kept separate from correctness tests because
they answer a different question and run on a different cadence.

This is where the project's headline performance claim is proven or disproven:
whole-chain pricing for 100 strikes with full Greeks in single-digit
microseconds on commodity Intel hardware — roughly 100x faster than QuantLib and
1000x faster than py_vollib doing the same work. Benchmarks here should compare
the scalar kernel vs the SIMD kernel vs the chain driver so the speedup from
vectorization is visible and regressions are caught.

Off by default; enable with `-DONITRO_BUILD_BENCHMARKS=ON`. Google Benchmark is
the suggested framework. Measure release builds only, and pin the CPU governor /
disable turbo for stable numbers (see Gil Tene "How NOT to Measure Latency").
