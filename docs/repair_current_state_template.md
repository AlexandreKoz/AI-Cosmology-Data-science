# Repair current-state recap template

_Date captured: YYYY-MM-DD (UTC)_

## Scope
- Task / ticket:
- Intended change boundary:

## Required preset evidence

### CPU-only debug
```bash
cmake --preset cpu-only-debug
cmake --build --preset build-cpu-debug
ctest --preset test-cpu-debug --output-on-failure
```
- Result: PASS / FAIL / BLOCKED
- Notes:

### HDF5 debug
```bash
cmake --preset hdf5-debug
cmake --build --preset build-hdf5-debug
ctest --preset test-hdf5-debug --output-on-failure
```
- Result: PASS / FAIL / BLOCKED
- Notes:

### PM + HDF5 + FFTW debug
```bash
cmake --preset pm-hdf5-fftw-debug
cmake --build --preset build-pm-hdf5-fftw-debug
ctest --preset test-pm-hdf5-fftw-debug --output-on-failure
```
- Result: PASS / FAIL / BLOCKED
- Notes:

## Blocker rule (mandatory)
If any required path fails or cannot run, include:
1. Exact failing command.
2. Exact terminal error.
3. Stop/blocked statement (do not claim completion).

## Layer-boundary check
Confirm whether core/config/state/provenance dependencies changed.
If yes, justify and reference an ADR update in `docs/architecture/decision_log.md`.

## Docs coupling check
List docs updated for any config/schema/restart/provenance/architecture change.
