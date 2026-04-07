# Repair open issues (P01–P19 freeze ledger)

_Date captured: 2026-04-07 (UTC)_

## Current blocker ledger after dependency-enabled validation

| ID | Status | Area | Exact file(s) | Command | Stage | Symptom |
|---|---|---|---|---|---|---|
| P19-GATE-FFTW-TEST-001 | Open | PM validation under HDF5+FFTW preset | `tests/unit/test_pm_solver.cpp:82` | `ctest --preset test-pm-hdf5-fftw-debug --output-on-failure` | Test | `unit_pm_solver` aborts: assertion `cosine_similarity > 0.98` failed. |
| P19-GATE-FFTW-TEST-002 | Open | TreePM periodic coupling validation | `tests/integration/test_integration_tree_pm_coupling_periodic.cpp` | `ctest --preset test-pm-hdf5-fftw-debug --output-on-failure` | Test | `integration_tree_pm_coupling_periodic` aborts with `TreePM periodic validation failed ... rel_l2=18129.9 (required <= 0.75)`. |

## Verified non-blocking evidence (this run)

- CPU-only path passes:
  - `cmake --preset cpu-only-debug`
  - `cmake --build --preset build-cpu-debug`
  - `ctest --preset test-cpu-debug --output-on-failure` (`36/36` passed)
- HDF5 path passes:
  - `cmake --preset hdf5-debug`
  - `cmake --build --preset build-hdf5-debug`
  - `ctest --preset test-hdf5-debug --output-on-failure` (`36/36` passed)
- `src/io/restart_checkpoint.cpp` compiles in dependency-enabled builds in this run; no current compile blocker reproduced there.

## Outcome tags

- **CPU-only preserved**
- **HDF5 path proven in this environment**
- **HDF5+FFTW blocked**
