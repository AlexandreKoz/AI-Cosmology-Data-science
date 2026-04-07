# Repair Closeout Report (P01–P19)

_Date: 2026-04-07 (UTC)_

## Gate decision

**STOP — Do not progress to P20.**

Dependency-enabled validation is not fully proven. CPU-only and HDF5 paths pass, but the HDF5+FFTW preset fails tests in the PM path.

## Validation outcomes

- **CPU-only preserved:** PASS (`36/36` tests).
- **HDF5 path:** PASS (`36/36` tests).
- **HDF5+FFTW path:** **BLOCKED** (`34/36` tests; 2 failures).

## Exact commands executed

```bash
cmake --preset cpu-only-debug
cmake --build --preset build-cpu-debug
ctest --preset test-cpu-debug --output-on-failure

cmake --preset hdf5-debug
cmake --build --preset build-hdf5-debug
ctest --preset test-hdf5-debug --output-on-failure

cmake --preset pm-hdf5-fftw-debug
cmake --build --preset build-pm-hdf5-fftw-debug
ctest --preset test-pm-hdf5-fftw-debug --output-on-failure
```

## Blocking evidence (HDF5+FFTW validation)

### Failure 1
- **Command:** `ctest --preset test-pm-hdf5-fftw-debug --output-on-failure`
- **Stage:** test
- **Test:** `unit_pm_solver`
- **Exact file:** `tests/unit/test_pm_solver.cpp:82`
- **Symptom:** assertion failure `cosine_similarity > 0.98`.

### Failure 2
- **Command:** `ctest --preset test-pm-hdf5-fftw-debug --output-on-failure`
- **Stage:** test
- **Test:** `integration_tree_pm_coupling_periodic`
- **Exact file:** runtime error reported from integration test executable
- **Symptom:** `TreePM periodic validation failed ... rel_l2=18129.9 (required <= 0.75)`.

## Compile blocker status for `src/io/restart_checkpoint.cpp`

- Configure/build completed successfully for:
  - `cpu-only-debug`
  - `hdf5-debug`
  - `pm-hdf5-fftw-debug`
- Therefore, **no unresolved compile blocker remains** in `src/io/restart_checkpoint.cpp` for the validated commands.

## Stabilization gate conclusion

Because the required HDF5+FFTW validation path did not pass, stabilization remains blocked at P19 and progression to P20 is **not authorized**.
