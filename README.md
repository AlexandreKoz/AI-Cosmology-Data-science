# CosmoSim

CosmoSim is a desktop-first and small-cluster-first cosmological simulation framework with an explicit scale-up path to larger HPC systems. This repository currently provides a **compilable architecture skeleton** that locks module boundaries, naming conventions, and build/test/benchmark scaffolding before heavy numerical kernels are added.

## Design summary

- Primary science target: physically credible zoom-in galaxy formation from cosmological initial conditions.
- Supported modes (planned): cosmological cubes, isolated galaxies, isolated clusters.
- Baseline numerical stack (planned): TreePM gravity, finite-volume Godunov hydro in comoving variables, patch AMR, hierarchical timesteps, modular baryonic physics.
- Architectural constraints: SoA-first persistent state, hot/cold separation, sidecars, reproducibility metadata, stable naming.

## Repository layout

- `include/cosmosim/` — public API headers by module.
- `src/` — internal implementation and ownership boundaries.
- `tests/unit/` — fast unit-level smoke checks.
- `tests/integration/` — configure/link/runtime integration smoke checks.
- `bench/` — lightweight benchmark/profiling hooks (`bench_*` convention).
- `configs/` — sample normalized runtime configuration files (including cosmo cube / zoom-in / isolated modes).
- `docs/` — architecture and developer placement guidance.
- `scripts/` — workflow helpers.
- `tools/` — standalone utility programs.
- `cmake/` — CMake helper modules and generated-header templates.

## Build and test

Preferred workflow uses presets:

```bash
cmake --preset cpu-only-debug
cmake --build --preset build-cpu-debug
ctest --preset test-cpu-debug
```

For dependency-enabled workflows and troubleshooting, see `docs/build_instructions.md`.

Manual workflow is still supported:

```bash
cmake -S . -B build/manual -DCMAKE_BUILD_TYPE=Debug
cmake --build build/manual
ctest --test-dir build/manual --output-on-failure
```

## Optional backend toggles

Feature gates are explicit and off by default unless your preset enables them:

- `COSMOSIM_ENABLE_MPI`
- `COSMOSIM_ENABLE_HDF5`
- `COSMOSIM_ENABLE_FFTW`
- `COSMOSIM_ENABLE_CUDA`
- `COSMOSIM_ENABLE_PYTHON`
- `COSMOSIM_ENABLE_TESTS`
- `COSMOSIM_ENABLE_BENCHMARKS`
- `COSMOSIM_ENABLE_LTO`

Configure produces:

- `cosmosim_feature_summary.txt` (human-readable feature report)
- `cosmosim_build_metadata.json` (machine-readable build metadata)

Both files are written into the active build directory.

## Presets

Canonical configure presets:

- `cpu-only-debug`
- `cpu-only-release`
- `hdf5-debug` (HDF5 I/O and schema/provenance work)
- `pm-hdf5-fftw-debug` (PM/TreePM validation path with FFTW + HDF5)
- `asan-debug`
- `mpi-release`
- `cuda-release`

`CMakeUserPresets.json.example` provides a local override template for custom compilers/toolchains.


## Python analysis bindings (optional)

When `COSMOSIM_ENABLE_PYTHON=ON`, CMake builds a pybind11 extension and stages a package at `build/<preset>/python/cosmosim`.

```bash
cmake -S . -B build/py -DCOSMOSIM_ENABLE_PYTHON=ON -DCOSMOSIM_ENABLE_HDF5=ON -Dpybind11_DIR=$(python3 -m pybind11 --cmakedir)
cmake --build build/py --target cosmosim_python_package
PYTHONPATH=build/py/python python3 -c "import cosmosim; print(cosmosim.__version__())"
```

See `docs/python_bindings_analysis.md` for API scope, ownership/copy semantics, and current limitations.

## New-file placement rule (for future prompts)

Before adding files, choose a module owner and place files by ownership:

1. Public API: `include/cosmosim/<module>/...`
2. Internal implementation: `src/<module>/...`
3. Internal-only helper headers: `src/<module>/internal/...`
4. Never place solver ownership in `utils` unless ownership is truly cross-cutting and narrow.

See `docs/developer_file_placement.md`, `docs/architecture.md`, and `docs/configuration.md` for more detail.
