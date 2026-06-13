# reference/

Your **trusted reference implementation** — the py_vollib-based (and optionally
QuantLib-based) code that ONITRO is validated against. This is a mentor-owned
correctness anchor, not part of the shipped C++ library.

## Why it's a top-level directory, not inside tests/

`tests/validation/` holds the *machinery* that compares ONITRO's output to a
reference (the C++ dump tool + the compare script). `reference/` holds the
*reference itself*: the actual py_vollib calls, the input grids, and the
expected-value tables you generate from them. Keeping it separate means:

- The "what is correct" lives in one obvious place, owned by you.
- The validation harness in `tests/validation/` just consumes whatever
  `reference/` produces — the two evolve independently.
- Mentees implementing a module can look here to see exactly what the right
  answer is for a given input, in readable Python, before debugging their C++.

## What goes here

- py_vollib pricing / Greeks / IV calls wrapped in small, readable functions.
- Input-grid generators (the strikes/vols/expiries you validate over).
- Scripts that emit expected-value tables (CSV/JSON) for the C++ side to check
  against, so the ground truth is version-controlled and reproducible.
- Optionally a QuantLib path for a second independent reference.

## Suggested layout (fill as you go)

    reference/
    ├── README.md
    ├── requirements.txt        py_vollib, QuantLib, numpy, pandas
    ├── pricing_reference.py     BSM / Black-76 prices + Greeks via py_vollib
    ├── iv_reference.py          implied vol via py_vollib
    ├── grids.py                 canonical input grids used everywhere
    └── expected/                generated expected-value tables (CSV/JSON)

Keep this in sync with `tests/validation/`: the compare script there reads the
tables this directory produces.
