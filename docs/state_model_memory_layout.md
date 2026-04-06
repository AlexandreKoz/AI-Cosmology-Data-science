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
