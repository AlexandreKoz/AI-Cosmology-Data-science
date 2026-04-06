# Restart and Checkpoint System

## Scope and schema

CosmoSim restart checkpoints are **exact-continuation artifacts** and intentionally richer than analysis snapshots.
The restart schema (`cosmosim_restart_v1`) persists:

- full `SimulationState` hot/cold SoA lanes,
- `StateMetadata` blob,
- module sidecars (`ModuleSidecarRegistry`) with per-module schema versions,
- `IntegratorState`,
- hierarchical scheduler persistent state (`TimeBinPersistentState`),
- normalized config text/hash and provenance payload,
- payload integrity hash (FNV-1a 64-bit).

By design, this differs from GADGET/AREPO-style analysis snapshots where scheduler internals and opaque sidecars are not mandatory.

## File format and compatibility

- Format: HDF5 (`writeRestartCheckpointHdf5`, `readRestartCheckpointHdf5`).
- Schema version gate: `isRestartSchemaCompatible(file_schema_version)`.
- Current compatibility policy: exact version match (`1`).

## Atomic write semantics

Writers always emit to `<final>.tmp` first and only then rename into final path.
This avoids clobbering a known-good restart with partial output.

## Integrity and provenance

- `restartPayloadIntegrityHash` hashes state+integrator+scheduler+config text/hash.
- Writer stores both integer and hex payload integrity hashes.
- Reader recomputes hash and rejects mismatches.
- Provenance is serialized with the checkpoint for continuation auditing.

## Parallel and scale-up note

The current implementation is single-rank write/read but keeps explicit scheduler/rank-related sidecars,
which preserves a direct path to coordinated rank-safe checkpointing in future MPI modes.
