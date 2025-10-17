# nanoSAT

**nanoSAT** is a truly minimal [CDCL](https://en.wikipedia.org/wiki/Conflict-driven_clause_learning) [SAT solver](https://en.wikipedia.org/wiki/Boolean_satisfiability_problem) for propositional logic formulas in [CNF](https://en.wikipedia.org/wiki/Conjunctive_normal_form). An example for a SAT instance in CNF is $(A \lor \lnot B) \land (\lnot A \lor B) \land A$ with the model $A = \mathrm{true}$ and $B = \mathrm{true}$.

Inspired by projects like [nanoGPT](https://github.com/karpathy/nanoGPT) and [minisat](https://github.com/niklasso/minisat), `nanoSAT` aims to provide an educational, readable, and modern implementation of a [Conflict-Driven Clause Learning](https://en.wikipedia.org/wiki/Conflict-driven_clause_learning) solver, without macros, custom allocators, union magic or other legacy C++ code.

A good conceptual starting point for understanding [CDCL](https://en.wikipedia.org/wiki/Conflict-driven_clause_learning) solvers can be found in [these lecture slides](https://satlecture.github.io/kit2024/) from a course at the Karlsruhe Institute of Technology (KIT).

We reuse small, relevant pieces from [minisat](https://github.com/niklasso/minisat) where appropriate, but everything has been modernized for C++20, simplified, and cleaned up.

## Features

* Pure STL — no custom allocators, pointer tricks, or union hacks.
* Modern C++20 code.
* Parser and solver are tested using [`googletest`](https://github.com/google/googletest/releases/tag/v1.17.0).
* Zero warnings on `gcc` and `clang` with `-Wall -pedantic`.

## Comparison

| | nanoSAT (ours) | [minisat-core](https://github.com/niklasso/minisat) | [kissat](https://github.com/arminbiere/kissat) |
|-|-|-|-|
| Lines of code (`cloc`) | 1,157 | 2,543 | 35,348 |
| Binary size | 140 kB | 1,568 kB | 556 kB |
| CMake builds | ✅ | ✅ | ❌ |
| Header-only library | ✅ | ❌ | ❌ |
| Modern C++ | ✅ (C++20) | ❌ | ❌ |
| No compiler warnings (`gcc` and `clang`) | ✅ | ❌ | ❌ |
| Only `std` containers | ✅ | ❌ | ❌ |
| No custom allocators & union magic | ✅ | ❌ | ❌ |
| Speed | 🐢 slow | 🐢 slow | 🐇 fast |

## Installation (Linux & macOS)

Clone the repository and configure dependencies:

```sh
sh configure.sh
```

Then build the project:

```sh
sh build_release.sh
```

Tested on `Linux Ubuntu 24.04` and `macOS Tahoe 26.0` with both `gcc 13.3` and `clang 17.0`.

## Example

Running `nanoSAT` on a problem instance with about 276,000 clauses requires about two minutes.

```sh
./build/nanosat tests/examples/success/hardware_verification.cnf.xz
```

An excerpt from the output of this command:

```txt
...

============================[      Summary      ]==============================
|                                                                             |
|  #Restarts:                     893                                         |
|  #Conflicts:                 374977 (    3226.183/sec)                      |
|  #Decisions:                6850813                                         |
|  #Propagations:           741740538 ( 6381699.134/sec)                      |
|  Total time:             116.229318                                         |
|                                                                             |
===============================================================================

SAT 1 -2 3 4 -5 -6 -7 ...
```

## Testing

To build and run all tests

```sh
sh build_release.sh && ./build/tests/nanosat-test
```
