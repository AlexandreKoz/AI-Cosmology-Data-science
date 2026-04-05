# CosmoSim architecture notes (skeleton stage)

## Layering

1. `core`: versioning, module registry, normalized config/provenance interfaces.
2. `gravity`: TreePM ownership boundary and force-kernel API surface.
3. `hydro`: finite-volume Godunov ownership boundary.
4. `amr`: patch lifecycle, refinement, and synchronization boundaries.
5. `physics`: modular baryonic source-term boundaries.
6. `io`: HDF5 schema adapters, stable snapshot/restart naming, provenance I/O.
7. `analysis`: diagnostics and analysis outputs.
8. `parallel`: MPI/OpenMP/GPU seams and execution policy boundaries.
9. `utils`: narrow cross-cutting helpers only.

## Storage and memory policy

- Persistent hot simulation state should favor structure-of-arrays.
- Cold metadata should be kept in sidecars to reduce hot-loop cache pressure.
- Active-set views should be compact and auditable.
- Ownership of hot solver data must remain with solver modules (`gravity`, `hydro`, `amr`) rather than generic helpers.

## Public/internal boundary rule

- All externally consumable headers are under `include/cosmosim/`.
- Internal headers are under `src/<module>/internal/`.
- Public headers can mention interfaces; internal headers hold implementation-side boundaries.

## Reproducibility rule

The skeleton reserves interfaces and docs for:

- normalized typed configuration
- provenance metadata emission
- schema versioning
- stable output naming for snapshots/restarts/tests/benchmarks

## Assumptions captured in this skeleton

- C++20 is the baseline language standard.
- Heavy kernels are intentionally deferred; only boundary declarations and smoke wiring are provided.
- Optional dependencies (MPI/OpenMP/CUDA/HDF5/FFTW) are compile-time toggles and may remain disabled.
