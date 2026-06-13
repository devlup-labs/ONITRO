# ONITRO

**Optimized Numerical Infrastructure for Trading, Risk and Options**

A fast, modern, SIMD-vectorized C++20 library for quantitative finance. ONITRO
covers the everyday computational primitives quant teams use: option pricing,
implied volatility solving, futures/bond pricing, Monte Carlo simulation, risk
metrics, and streaming statistics. Python bindings (deferred to the final phase)
will expose the whole library to the Jupyter-first quant workflow.

ONITRO is a **calculator, not a forecaster** — it computes fair values and
sensitivities given inputs. It does not predict prices.

## Modules

| # | Module        | Directory                  | Owner  |
|---|---------------|----------------------------|--------|
| 1 | Pricing core  | `pricing/` + `greeks/`     | Parrva |
| 2 | Implied vol   | `iv/`                      | Parrva |
| 3 | Instruments   | `instruments/`             | Arnav  |
| 4 | Monte Carlo   | `montecarlo/`              | Parrva |
| 5 | Risk          | `risk/`                    | Arnav  |
| 6 | Stats         | `stats/`                   | Arnav  |
| 7 | Python bindings | _(deferred to final phase)_ |      |

The shared numerical primitive (cumulative normal, etc.) lives in `math/`, which
every module may depend on and which depends on nothing else.

## Layout

```
include/onitro/<module>/   public headers (you create these per module)
src/<module>/              implementation, compiled into the onitro static lib
tests/unit/                fast C++ unit tests, no Python — run on every build
tests/validation/          cross-language ground-truth sweep vs py_vollib/QuantLib
reference/                 trusted py_vollib/QuantLib reference (the "right answer")
benchmarks/                latency/throughput benchmarks (the chain-pricing target)
examples/                  minimal end-to-end usage programs
docs/                      architecture and API-convention decisions
```

The eight `include`/`src` subdirectories map to the six modules: `math/` is the
shared primitive (depends on nothing); `pricing/`+`greeks/` are Module 1; `iv/`
is Module 2; `instruments/` is Module 3; `montecarlo/` is Module 4; `risk/` is
Module 5; `stats/` is Module 6. Module 7 (Python bindings) is deferred and will
arrive as a new top-level `bindings/` directory.

The directories ship empty — **mentees decide how to break each module into
files.** Start each module from the architecture rules below, not from a
pre-made skeleton.

## Architecture (read before contributing)

Two decisions are mandatory and established on day one. See
[`docs/architecture.md`](docs/architecture.md):

1. **Struct-of-arrays (SoA) layout** for all chain data — required for SIMD.
2. **Three-layer separation** per module — scalar kernel, SIMD kernel, chain
   driver — as sectioned functions within each module's files.

Cross-module boundaries are explicit and stable; modules do not reach into each
other's internals. See [`docs/api_conventions.md`](docs/api_conventions.md).

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Requires a C++20 compiler (GCC 12+ or Clang 15+), CMake 3.20+, and a CPU with
AVX2 for the vectorized paths. The library target stays dormant until the first
source file is listed in `ONITRO_SOURCES` (top of `CMakeLists.txt`) — the
natural first file is the shared `math/` primitive. Once sources exist,
`ctest --test-dir build` runs the C++ unit tests.

## License

MIT. See [`LICENSE`](LICENSE).
