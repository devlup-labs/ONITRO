# ONITRO Internal API Conventions

The contract module owners code against. Tentative until ratified in Week 2, but
this is the direction. The point is that the Module 1 and Module 2 owners (and
the Module 5 owner consuming Module 1's Greeks) are not guessing at boundaries.

## Numeric types

- `double` throughout. Optional `float` templating only in proven-hot SIMD paths.

## Array interfaces

- Inputs: `std::span<const double>`. Outputs: `std::span<double>`.
- **No `std::vector` in interfaces** — allocation is the caller's business and
  stays hidden from the hot path.
- Chain functions take parallel equal-length spans (SoA), one per field.

## Greeks

- `greeks::Greeks` is an aggregate of `delta, gamma, vega, theta, rho`.
- Chain Greeks are returned struct-of-arrays (`GreeksChain`, a struct of output
  spans), never array-of-structs.

## Option type / call-put dispatch

- `pricing::OptionType` is an enum whose underlying value **is** the sign factor
  (`Call = +1`, `Put = -1`), so the branch-free formulas multiply by it
  directly.

## Error reporting

- **No exceptions in hot paths.** Numerical failures use sentinel `NaN` (e.g.
  IV below intrinsic, above arbitrage bound). A `std::expected`-style result
  type is acceptable for non-hot, configuration-level errors.

## Module dependency rules

- `math/` depends on nothing; everyone may depend on it.
- `pricing/` + `greeks/` (Module 1) depend only on `math/`.
- `iv/` (Module 2) depends on `pricing/` (needs price + vega).
- `montecarlo/` (Module 4) depends on `math/` (and `pricing/` only for
  cross-checking, not at runtime).
- `risk/` (Module 5) depends on `greeks/` for portfolio Greeks aggregation —
  the main cross-module boundary on the co-mentor's side. Integrate ~Week 8+.
- `stats/` (Module 6) depends on nothing outside `math/`.
- No module reaches into another module's `src/` internals; only public headers.
