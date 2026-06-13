# benchmarks/CMakeLists.txt
#
# Performance measurement, OFF by default (-DONITRO_BUILD_BENCHMARKS=ON).
# Recommended framework: Google Benchmark. Add one target per benchmark, e.g.:
#
#   add_executable(bench_pricing bench_pricing.cpp)
#   target_link_libraries(bench_pricing PRIVATE onitro::onitro benchmark::benchmark)
#
# Empty until there is a hot path worth measuring.
