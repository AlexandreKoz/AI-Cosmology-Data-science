# Repair state recap (frozen)

_Date captured: 2026-04-07 (UTC)_

This document freezes the repository state **before any new repair** and records command-backed evidence for build/test behavior on CPU-only and HDF5-enabled paths.

## 1) Repository snapshot

### 1.1 Root tree indicators

Command:

```bash
find . -maxdepth 1 -type f | sed 's#^./##' | sort
```

Observed root-level files:

- `.gitignore`
- `CMakeLists.txt`
- `CMakePresets.json`
- `CMakeUserPresets.json.example`
- `Dados climáticos_teste.xlsx`
- `README.md`

Naming rule reference (`docs/developer_file_placement.md`): files/directories must be `lower_snake`. The root filename `Dados climáticos_teste.xlsx` violates this convention (uppercase initial + diacritic + space-like naming style mismatch for repo policy).

## 2) Presets and feature-path coverage state

### 2.1 Current configure/build presets in tree

Command:

```bash
cat CMakePresets.json
```

Current configure presets present:

- `cpu-only-debug`
- `python-debug`
- `cpu-only-release`
- `asan-debug`
- `mpi-release`
- `cuda-release`

Current gap: there is **no dedicated** preset for:

- HDF5-enabled CPU path
- HDF5+FFTW-enabled path

### 2.2 Dependency discovery logic (CMake)

Command:

```bash
nl -ba CMakeLists.txt | sed -n '55,67p'
```

Observed:

- HDF5: `find_package(HDF5 REQUIRED COMPONENTS C CXX)` gated by `COSMOSIM_ENABLE_HDF5`.
- FFTW: `find_package(PkgConfig REQUIRED)` + `pkg_check_modules(FFTW REQUIRED IMPORTED_TARGET fftw3)` gated by `COSMOSIM_ENABLE_FFTW`.

This confirms discovery exists, but preset coverage for HDF5/HDF5+FFTW is missing.

## 3) CPU-only (default path) evidence

### 3.1 Configure

Command:

```bash
cmake --preset cpu-only-debug
```

Outcome: **PASS**. Feature summary reports `feature_hdf5=false`, `feature_fftw=false`.

### 3.2 Build

Command:

```bash
cmake --build --preset build-cpu-debug
```

Outcome: **PASS** (full target graph built, including tests and benchmarks).

### 3.3 Tests

Command:

```bash
ctest --preset test-cpu-debug
```

Outcome: **PASS** (`34/34` tests passed).

Interpretation: CPU-only/default path is currently healthy and remains a preserved win.

## 4) HDF5-enabled feature-path evidence

## 4.1 Initial dependency behavior (before installing HDF5 dev package)

Command:

```bash
cmake -S . -B build/hdf5-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCOSMOSIM_ENABLE_TESTS=ON \
  -DCOSMOSIM_ENABLE_BENCHMARKS=ON \
  -DCOSMOSIM_ENABLE_HDF5=ON \
  -DCOSMOSIM_ENABLE_FFTW=OFF
```

Outcome (first run): configure failed with `Could NOT find HDF5`.

This is actionable dependency behavior (explicit failure when enabled dependency is missing).

## 4.2 HDF5-enabled configure/build after HDF5 package available

Commands:

```bash
apt-get update && apt-get install -y libhdf5-dev
cmake -S . -B build/hdf5-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCOSMOSIM_ENABLE_TESTS=ON \
  -DCOSMOSIM_ENABLE_BENCHMARKS=ON \
  -DCOSMOSIM_ENABLE_HDF5=ON \
  -DCOSMOSIM_ENABLE_FFTW=OFF
cmake --build build/hdf5-debug
```

Outcome:

- Configure: **PASS** (`Found HDF5 ... version 1.10.10`).
- Build: **FAIL** in `src/io/restart_checkpoint.cpp`.

## 4.3 Exact compile failure class in `src/io/restart_checkpoint.cpp`

Primary symptom class:

- Template mismatch when writing aligned vectors to helper that only accepts `const std::vector<T>&`.
- Assignment mismatch when reading `std::vector<T>` datasets into `core::AlignedVector<T>` fields.

Representative compiler diagnostics from failed build:

- `no matching function for call to writeDataset1d(... cosmosim::core::AlignedVector<...>&)`
- `mismatched types 'std::allocator<_CharT>' and 'cosmosim::core::AlignedAllocator<..., 64>'`
- `no match for 'operator=' ... AlignedVector<T> ... and std::vector<T>`

Affected lines are in the HDF5 read/write state-group section of `src/io/restart_checkpoint.cpp` (e.g., around line 243 onward for writes, 324 onward for reads).

## 5) IC reader warning path evidence

Command:

```bash
nl -ba src/io/ic_reader.cpp | sed -n '490,493p'
```

Observed warning path:

- The IC reader appends unsupported field warning:
  - `"PartType0 thermodynamic fields currently bypassed by IC reader; hydro sidecar defaults preserved"`

This confirms PartType0 thermodynamic ingestion is intentionally bypassed in current state.

## 6) Missing docs evidence

Command:

```bash
for f in docs/build_instructions.md CONTRIBUTING.md; do
  if [ -f "$f" ]; then echo "FOUND $f"; else echo "MISSING $f"; fi
 done
```

Outcome:

- `MISSING docs/build_instructions.md`
- `MISSING CONTRIBUTING.md`

## 7) Public interface freeze baseline

Command:

```bash
find include/cosmosim -type f | sort
```

Current public interface set is frozen at the current headers under:

- `include/cosmosim/core/*`
- `include/cosmosim/gravity/*`
- `include/cosmosim/hydro/*`
- `include/cosmosim/io/*`
- `include/cosmosim/amr/*`, `analysis/*`, `parallel/*`, `physics/*`, `utils/*`
- umbrella `include/cosmosim/cosmosim.hpp`

No public interface edits were made in this capture step.

## 8) What is stable vs. blocked at freeze point

### Stable/preserved

- CPU-only preset configure/build/test path is passing.
- Existing architecture wins remain present in tree (SoA + hot/cold/sidecar structures, typed config, provenance, staged integrator docs/contracts).

### Blocked

- HDF5-enabled build path fails at compile time in `restart_checkpoint` due to allocator/container compatibility assumptions.
- IC reader still bypasses PartType0 thermodynamic fields.
- Dedicated HDF5 / HDF5+FFTW presets absent.
- Expected contributor/build docs absent.
- Root-level filename naming-convention violation remains.
