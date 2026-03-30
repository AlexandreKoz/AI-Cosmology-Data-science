# CosmoSim Core Data Model (Phase-I deterministic SoA contract)

## Engineering restatement
Design a restart-safe, deterministic, structure-of-arrays state model that keeps hot kernel data compact, pushes optional physics into sidecar arrays, and exposes active-set views suitable for CPU OpenMP today and MPI/GPU decomposition later.

## Governing equations and definitions used by this design
CosmoSim Phase-I master design does not freeze full gravity/hydrodynamics equations yet. This data model therefore commits only to variables that are equation-agnostic and already policy-approved:

- Kinematic advection variable family
  - `x_comoving_mpc, y_comoving_mpc, z_comoving_mpc`
  - `vx_peculiar_kms, vy_peculiar_kms, vz_peculiar_kms`
- Mass/state variables in code units
  - `mass_code`, `rho_code`, `u_thermal_code`
- Cosmology clock state
  - `scale_factor_a`, `hubble_H_code`

Ambiguity statement: exact force normalization, time variable coupling (`a` vs `t` vs conformal time), and hydro closure forms remain external to this file and must be attached later in module physics docs.

## Master state containers
- `SimState`
  - `ParticleSpeciesState`
    - `hot_dm: HotParticleSoA`
    - `hot_star: HotStarSoA`
    - `hot_bh: HotBhSoA`
    - `cold_dm/cold_star/cold_bh: ColdEntitySidecar`
    - `sidecar_dm/sidecar_star/sidecar_bh: OptionalModuleSidecars`
  - `CellSpeciesState`
    - `hot_gas: HotGasCellSoA`
    - `cold_gas: ColdEntitySidecar`
    - `sidecar_gas: OptionalModuleSidecars`
  - `ActiveSetIndex`
    - dense active subsets: `particle_dm_index`, `cell_gas_index`, `particle_star_index`, `particle_bh_index`
  - `DeterministicIndexState`
    - bidirectional slot maps (`slot_to_dense_*`, `dense_to_slot_*`) for stable restart ordering
  - scalar run state: `scale_factor_a`, `hubble_H_code`, `step_id`

## Ownership rules
- `core` owns all hot/cold master arrays and deterministic index maps.
- Physics modules own only optional sidecar semantics; allocation keys remain in `core` index space.
- Runtime may reorder dense hot storage for compaction only if `DeterministicIndexState` is atomically updated in the same operation.
- I/O must serialize by stable `persistent_id` and checkpoint slot map.
- Analysis receives read-only views and cannot mutate hot or cold arrays.

## Index-stability rules
1. `persistent_id` is immutable for the life of an entity.
2. `restart_slot` is the canonical deterministic ordering key in checkpoint files.
3. Dense index (`0..N-1`) is execution-local and may change between steps due to compaction.
4. Every dense reordering must update both `slot_to_dense_*` and `dense_to_slot_*` maps.
5. Active-set vectors contain dense indices only; stable IDs are resolved through cold sidecars.
6. Entity deletion sets tombstone semantics in slot maps; slot reuse must be explicit and logged.

## Particle/cell/species separation policy
- Dark matter, stars, and BHs are particle species with independent hot arrays to avoid branch-heavy mixed loops.
- Gas is cell-centered (`HotGasCellSoA`) and kept separate from particle species even if represented by moving-mesh/SPH-like operators.
- Cross-species interactions operate through explicit kernel adapters, never by fusing master SoA containers into a tagged union in hot loops.

## Hot kernel views
- Gravity: `GravityKernelView`
  - active DM index + position + mass.
- Hydro: `HydroKernelView`
  - active gas index + density/internal energy + velocity.
- AMR: `AmrKernelView`
  - active gas index + density + smoothing/refinement scale.
- IO: `IoSnapshotView`
  - stable `persistent_id` + canonical kinematic and mass arrays.
- Analysis: `AnalysisView`
  - stable identifiers + restart slot + mass.

These views are non-owning spans and can be rebound to MPI-local chunks or GPU device buffers later without changing solver signatures.

## Fields that must never live in hot core state
- Provenance and debug flags beyond minimal kernel gating bits.
- Chemistry network abundances and reaction rates.
- MHD magnetic fields in baseline mode.
- Feedback history accumulators (SN/AGN counters, delayed-cooling timers).
- Halo/subhalo catalog linkage.
- Group finder scratch data.
- Any string labels, UUID text, file-path fragments.
- Long-timescale analysis accumulators.
- Checkpoint manifests and schema/version metadata.

## Byte-budget estimates per entity
Assume current Phase-I scalar precision policy in code: `float` for hot and optional sidecar numeric fields, fixed-width integers for cold metadata.

| Entity | Mode | Hot bytes | Cold bytes | Sidecar bytes | Total bytes |
|---|---:|---:|---:|---:|---:|
| DM particle | baseline | 32 | 15 | 12 | 59 |
| DM particle | flagship | 32 | 15 | 17 | 64 |
| Gas cell | baseline | 40 | 15 | 4 | 59 |
| Gas cell | flagship | 40 | 15 | 17 | 72 |
| Star particle | baseline | 28 | 15 | 4 | 47 |
| Star particle | flagship | 28 | 15 | 5 | 48 |
| BH particle | baseline | 32 | 15 | 1 | 48 |
| BH particle | flagship | 32 | 15 | 5 | 52 |

Notes:
- Cold metadata sum (`15`) is raw field-width and does not include allocator padding/alignment overhead.
- Baseline sidecar set keeps only required acceleration or metallicity hooks.
- Flagship sidecar set includes chemistry, optional MHD vectors, and feedback flags.

## MPI/GPU compatibility hooks
- Active-set vectors can be interpreted as local-domain indices under MPI domain decomposition.
- SoA field vectors can be wrapped in execution-policy buffers (`std::span` now, backend-specific views later).
- Sidecar isolation prevents optional physics payload from inflating mandatory halo exchange packets.
