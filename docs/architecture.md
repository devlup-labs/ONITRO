# ONITRO Architecture

This document pins the two design decisions that **must** hold from day one,
because retrofitting either is expensive. New contributors read this before
writing code; reviewers point at it.

## 1. Struct-of-arrays (SoA) layout

All chain-shaped data is stored as **separate contiguous arrays per field**, not
as an array of per-option structs.

```
// YES (SoA): each field contiguous, vectorizes cleanly.
std::span<const double> strikes, vols, expiries, ...;

// NO (AoS): fields interleaved, defeats SIMD, wastes cache lines.
struct Opt { double K, sigma, T, ...; };  std::span<const Opt> chain;
```

**Why:** AVX2 loads 4 contiguous doubles into one register. SoA makes "the next
four strikes" a single aligned load; AoS scatters them across memory. This is a
*design decision, not a language feature* — and it's the single thing most
expensive to change later, because it touches every signature. Prove it to
yourself on Compiler Explorer (`-O3 -mavx2`): only the SoA loop vectorizes.

## 2. Three-layer separation per module

Every compute module is structured in three layers, as clearly-sectioned
functions **within** the module's files (not as separate directories):

1. **Scalar kernel** — one input, one output. Correct and readable. The thing
   the SIMD kernel and the validation sweep are checked against.
2. **SIMD kernel** — AVX2, 4 lanes. Same math, vectorized. An internal detail
   of the `.cpp`; not part of the public header.
3. **Chain driver** — the public API. Loops over the data in steps of 4, calls
   the SIMD kernel, handles the ragged tail (size not divisible by 4) with a
   masked or scalar remainder.

**Why:** the scalar kernel gives you a trusted reference and a place to reason
about the math without SIMD noise. The split means vectorization is a *rewrite
of one layer*, never a redesign of the module.

## Supporting rules

- **Branch-free hot paths.** Replace `if` with arithmetic/masking in
  performance-critical code. Call/put dispatch uses a sign factor, not a branch
  — an unpredictable branch costs ~15–20 cycles.
- **Allocation-free hot paths.** Pre-allocate; the per-call/per-tick path never
  calls `malloc`. Interfaces take `std::span`, hiding allocation from callers.
- **Isolate hard numerical primitives.** The cumulative normal lives in `math/`,
  depends on nothing, and is validated as a standalone unit before anything
  trusts it. Apply the same to any other tricky primitive.
- **Compose, don't conflate.** Modules don't reach into each other's internals.
  `pricing/` knows nothing about `montecarlo/`; `risk/` knows nothing about the
  IV solver. Cross-module boundaries are explicit (see `api_conventions.md`).

## The reusable decision sequence

When designing any new module, in order:

1. **Pin the data layout** (SoA for anything chain-shaped).
2. **Separate the layers** (scalar / SIMD / driver).
3. **Eliminate branching** in the hot path (arithmetic over conditionals).
4. **Isolate the hard numerical primitive** (own file, own tests).
