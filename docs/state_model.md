# Core state model and memory layout

This document defines the baseline ownership and memory contract for simulation state.

## Persistent state ownership

`cosmosim::core::SimulationState` is the single persistent owner for run state arrays.

- Particle arrays are owned per species via `ParticleSpeciesState` (`dark_matter`, `gas`, `star`, `black_hole`).
- Cell arrays are owned by `CellSoa`.
- Patch arrays are owned by `PatchSoa`.
- Module-owned extension payloads are explicit `ModuleSidecar` entries keyed by module name.

All central arrays are structure-of-arrays (SoA), with hot solver fields separated from sidecar metadata.

## Transient workspace ownership

`cosmosim::core::StepWorkspace` owns all per-step transient memory:

- `ScratchAllocator` interface with `MonotonicScratchAllocator` default.
- Gather/scatter helper vectors for compact active sets.

Transient buffers are reset per step and are intentionally not owned by `SimulationState`.

## Active-set contract

`ActiveIndexSet` stores compact contiguous local indices for active entities.
Kernels consume `std::span<const std::uint32_t>` views from this set.

## Schema and provenance implications

`SimulationMetadata` stores schema versioning and normalized provenance fields:

- `schema_version`
- `run_name`
- `config_digest`
- `initial_conditions_source`
- `normalized_config_text`
- `provenance_stamp`

`normalizedDump()` provides a stable key/value emission order suitable for run logs, restart headers, and output metadata.

## Conservative assumptions

- Species use a fixed canonical order and local contiguous indices.
- Sidecar payloads are byte-packed with explicit element width.
- Scratch allocations require power-of-two alignment values.
