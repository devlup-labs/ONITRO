# examples/

Small, self-contained programs that demonstrate **how a user uses the library**,
linked against the compiled `onitro` lib.

These are not tests and not benchmarks — they are documentation that compiles.
The goal of each example is to read like the shortest believable real usage:
"price a whole option chain in a few lines", "solve implied vol across strikes",
"run a Monte Carlo European option and print the confidence interval". When the
Python bindings land, the examples double as the basis for the README snippets
and the demo notebook.

Rule of thumb: if a new contributor would copy it to get started, it belongs
here. If it asserts correctness, it belongs in `tests/`. If it measures speed,
it belongs in `benchmarks/`.
