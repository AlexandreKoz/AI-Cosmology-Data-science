# Build Instructions

This project is in repair-first stabilization mode. Prefer CMake presets over ad-hoc cache edits so build intent is reproducible.

## Preset guide (what to use)

- **Default developer preset:** `cpu-only-debug`
  - Use for most core/state/integrator work.
  - No optional dependency requirements beyond a C++20 compiler + CMake + Ninja.
- **HDF5 preset:** `hdf5-debug`
  - Use for snapshot/restart/provenance schema checks and HDF5 I/O behavior.
  - Requires HDF5 C/C++ development libraries.
- **PM/TreePM validation preset:** `pm-hdf5-fftw-debug`
  - Use for PM/TreePM validation when FFTW-backed PM behavior and HDF5 I/O are both needed.
  - Requires HDF5 plus FFTW3 discoverable through `pkg-config`.

## 1) Beginner CPU-only path

```bash
cmake --preset cpu-only-debug
cmake --build --preset build-cpu-debug
ctest --preset test-cpu-debug
```

## 2) Feature-enabled HDF5 path

```bash
cmake --preset hdf5-debug
cmake --build --preset build-hdf5-debug
ctest --preset test-hdf5-debug
```

If configure fails with missing HDF5, install HDF5 development packages and/or point CMake at your install:

```bash
cmake --preset hdf5-debug -DHDF5_ROOT=/path/to/hdf5
```

## 3) PM/TreePM + HDF5 + FFTW validation path

```bash
cmake --preset pm-hdf5-fftw-debug
cmake --build --preset build-pm-hdf5-fftw-debug
ctest --preset test-pm-hdf5-fftw-debug
```

If configure fails with missing FFTW, install FFTW development packages and ensure `fftw3.pc` is visible to `pkg-config`:

```bash
cmake --preset pm-hdf5-fftw-debug -DPKG_CONFIG_PATH=/path/to/fftw/lib/pkgconfig
```

## Local user overrides

Copy `CMakeUserPresets.json.example` to `CMakeUserPresets.json` for workstation-specific paths/toolchains. Do not commit `CMakeUserPresets.json`.

## Notes on dependency behavior

- Optional dependencies are **opt-in** via preset/feature flags.
- If an enabled dependency is missing, configure now fails fast with actionable error text (no silent fallback).
