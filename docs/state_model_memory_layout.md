# Core Data Model and Memory Layout

This document defines the persistent and transient memory contract for simulation state.

## Persistent ownership (`SimulationState`)

`SimulationState` is the single ownership root for persistent run state:

- `ParticleSoa`: hot particle fields in structure-of-arrays form.
- `ParticleSidecar`: cold particle metadata (`particle_id`, species tags, ownership flags).
- `CellSoa`: hot finite-volume cell fields.
- `PatchSoa`: AMR patch descriptors and contiguous cell ranges.
- `SpeciesContainer`: explicit species accounting for invariant checks.
- `StateMetadata`: schema/provenance-sensitive run metadata.
- `ModuleSidecarRegistry`: module-specific persistent payload blocks.

The ownership invariant is validated by `SimulationState::validateOwnershipInvariants()`.

## Transient ownership (`TransientStepWorkspace`)

`TransientStepWorkspace` owns temporary compact arrays for active solver subsets and a
`MonotonicScratchAllocator` for scratch bytes. This workspace is explicitly resettable and does
not retain persistent simulation data.

## Active-set views

`buildParticleActiveView` and `buildCellActiveView` materialize compact, contiguous active buffers
from index lists. Returned views are `std::span`-based and stable for the lifetime of the workspace
storage.

## Reproducibility and schema implications

`StateMetadata` provides a deterministic key/value serialization surface for schema version, run
identity, snapshot/restart naming, step index, and normalized config hash fields.

Conservative assumptions:

1. Metadata serialization format is line-based key/value text and intentionally minimal.
2. Species tags are encoded as a bounded integer enum (0..4).
3. Patch-to-cell mapping uses contiguous ranges (`first_cell`, `cell_count`) for locality and future
   MPI packing.

## Reusable SoA substrate (`soa_storage.hpp`)

`core/soa_storage.hpp` adds a reusable SoA substrate built around aligned contiguous field arrays,
field-keyed typed span access, and gather/scatter helpers for active kernels:

- `SoaFieldArray<T>`: aligned contiguous field storage with explicit `size`, `capacity`,
  `reserve`, `resize`, `swapErase`, and stable compaction.
- `ParticleSoaStorage`: canonical particle-oriented field pack (`pos_*`, `vel_*`, `mass`, `id`,
  `rho`, `u_int`) with typed access via `ParticleSoaField`.
- `gatherSpan` / `scatterSpan`: index-driven data movement for active kernels.

Schema/provenance implications:

1. This change is in-memory only and introduces no snapshot schema rename.
2. Canonical external naming remains unchanged in configuration and restart metadata.
3. The substrate keeps host-side semantics compatible with future device mirrors by using
   per-field contiguous arrays and explicit logical sizes.
