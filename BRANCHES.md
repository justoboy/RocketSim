# Branches

This repository carries the RocketSim simulation source on two branches that must stay in
behavioral parity:

| Branch        | Purpose                                                                                                   | Build artifact            |
|---------------|-----------------------------------------------------------------------------------------------------------|---------------------------|
| `main`        | Canonical source of truth. Includes the Python binding layer (`python-mtheall/`, `cmeel/`, `pyproject.toml`). | Python extension module |
| `cpp-standalone` | C++-only distribution for pure-C++ consumers (e.g. `RLGymSim_CPP`). Same `src/` + `libsrc/` as `main`, with the Python layer removed and a standalone static-library `CMakeLists.txt`. | `RocketSim` static lib    |

## Parity rule

`cpp-standalone` is **derived from `main`** and must never diverge in simulation behavior.
The only intentional differences on `cpp-standalone` are:

1. Deleted: `python-mtheall/`, `cmeel/`, `RocketSim.pyi`, `MANIFEST.in`, `pyproject.toml`, `.github/`.
2. Replaced: `CMakeLists.txt` — a plain `add_library(RocketSim STATIC ...)` over `src/` +
   `libsrc/bullet3-3.24/` with `RS_DONT_LOG=1`, instead of the `Python3_add_library(MODULE)` build.

Everything under `src/` and `libsrc/` is byte-identical between the two branches.

## How to change the simulation (keep both branches in sync)

1. Make the change on `main` (the source of truth) and commit.
2. Propagate it onto `cpp-standalone`:

   ```bash
   git checkout cpp-standalone
   git checkout main -- src libsrc
   git commit -m "sync: propagate <change> from main"
   ```

   (`git checkout main -- src libsrc` copies the exact tracked contents of those directories from
   `main`, which is the whole parity contract.)
3. Push both branches.

Never edit `src/` or `libsrc/` directly on `cpp-standalone`; always edit `main` first, then sync.
