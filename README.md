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
- `configs/` — sample normalized runtime configuration files.
- `docs/` — architecture and developer placement guidance.
- `scripts/` — workflow helpers.
- `tools/` — standalone utility programs.
- `cmake/` — future CMake helper modules.

## Build and test

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Optional backend toggles

CMake options are present and off by default:

- `COSMOSIM_ENABLE_MPI`
- `COSMOSIM_ENABLE_OPENMP`
- `COSMOSIM_ENABLE_CUDA`
- `COSMOSIM_ENABLE_HDF5`
- `COSMOSIM_ENABLE_FFTW`

These options are placeholders for future modules and enforce explicit dependency activation.

## New-file placement rule (for future prompts)

Before adding files, choose a module owner and place files by ownership:

1. Public API: `include/cosmosim/<module>/...`
2. Internal implementation: `src/<module>/...`
3. Internal-only helper headers: `src/<module>/internal/...`
4. Never place solver ownership in `utils` unless ownership is truly cross-cutting and narrow.

See `docs/developer_file_placement.md` and `docs/architecture.md` for more detail.
