# CosmoSim Phase-I Repository Architecture (Desktop + Small Cluster)

## Engineering restatement
Design an immediately implementable repository skeleton for a CPU-first cosmological simulation framework with strict SoA data layout, hot/cold split, sidecar optional physics, deterministic provenance, and modern CMake.

## Governing numerics and variable policy
- Phase-I integrator placeholder: drift step in DKD pattern, where position-like fields are advanced by velocity-like fields over `dt_code`.
- **Unit policy**: no implicit SI/CGS conversion in kernels. Fields are named with unit suffixes (`_comoving_mpc`, `_peculiar_kms`, `_code`) and all modules must preserve suffix discipline.
- **Ambiguity statement**: Phase-I leaves exact cosmological time variable (`a`, `t`, `\tau`) and Poisson/source normalization unspecified. Chosen policy is to keep scheduler APIs in code units (`dt_code`) and document conversion hooks in `physics/` before production gravity/hydro coupling.

## Top-level directory tree
```text
CosmoSim/
├── CMakeLists.txt
├── cmake/
├── config/
│   ├── cosmology/
│   ├── numerics/
│   └── runtime/
├── docs/
│   ├── architecture/
│   ├── provenance/
│   └── testing/
├── include/cosmosim/
│   ├── core/
│   ├── gravity/
│   ├── hydro/
│   ├── amr/
│   ├── io/
│   ├── analysis/
│   ├── physics/
│   ├── runtime/
│   └── utils/
├── src/
│   ├── core/
│   ├── gravity/
│   ├── hydro/
│   ├── amr/
│   ├── io/
│   ├── analysis/
│   ├── physics/
│   ├── runtime/
│   └── utils/
├── tests/
│   ├── unit/
│   ├── integration/
│   ├── regression/
│   └── performance/
├── runs/
├── scripts/
├── tools/
└── third_party/
```

## Directory responsibilities
- `include/cosmosim/*`: stable public headers only, module facades, POD layout contracts.
- `src/*`: internal implementations; no cross-module internal include leakage.
- `core/`: SoA storage, active-set compaction, hot/cold ownership, deterministic state transitions.
- `runtime/`: stepping orchestration, backend policy (serial/OpenMP now; GPU adapter seam later).
- `gravity|hydro|amr|physics`: numerics kernels and model closures with sidecar access only via explicit handles.
- `io/`: HDF5 adapters, frozen field-name mapping, checkpoint/restart.
- `analysis/`: derived diagnostics over immutable snapshots, never mutating hot state.
- `config/`: run schemas, defaults, validated parameter sets.
- `docs/`: architecture, provenance semantics, test baselines.
- `runs/`: run artifacts (`run_<id>/`, logs, manifests, checkpoints) under naming conventions.

## Public vs internal API boundaries
- Public API = headers under `include/cosmosim/**`. Consumers may instantiate `SimState`, call scheduler entrypoints, and invoke I/O/analysis facades.
- Internal API = anything under `src/**` and private headers under `src/**/detail`. Not imported across module boundaries.
- Boundary rule: module A may include module B public headers only; never B implementation headers.

## Dependency direction rules
1. `utils` -> none.
2. `core` -> `utils`.
3. `physics` -> `core`, `utils`.
4. `gravity|hydro|amr` -> `core`, `physics`, `utils`.
5. `runtime` -> `core`, `gravity`, `hydro`, `amr`, `physics`, `utils`.
6. `io` -> `core`, `utils` (no dependency on solvers).
7. `analysis` -> `core`, `io`, `utils` (read-only semantics).
8. `main/driver` -> public APIs from all modules.

No cyclic dependencies. Optional features use registration tables and sidecars, not direct module back-links.

## Data layout and ownership (SoA + hot/cold + sidecar)
- `core::ParticleHotSoA`: dense arrays for per-step touched fields (positions, velocities, masses).
- `core::ParticleColdSidecar`: sparse/rarely used metadata (persistent IDs, provenance flags).
- Optional physics (e.g., chemistry, MHD tracers, feedback flags) must live in module-owned sidecars keyed by stable particle/cell IDs, not in hot structs.
- Active-set compaction happens in `core` only; modules consume compact views and return masks/indices.

## Module ownership matrix
| Module | Owns | Reads | Writes | Forbidden |
|---|---|---|---|---|
| gravity | potential/acceleration work buffers | hot mass/position | acceleration sidecar or hot accel arrays | I/O format decisions |
| hydro | primitive/conserved cell SoA, Riemann scratch | mesh/cell hot arrays | hydro flux/state arrays | touching gravity internals |
| amr | block hierarchy metadata, refinement flags | hydro/gravity error indicators | mesh topology | storing optional physics in hot block structs |
| io | schema map, checkpoint streams | all public state views | files only | modifying simulation state semantics |
| analysis | diagnostic accumulators | snapshots/checkpoints | reports/derived data | mutating integrator state |
| config | validated parameter structs | text/yaml/json inputs | immutable runtime config | runtime mutation after lock |
| physics | equation-of-state/closure params, unit transforms | config + core views | closure outputs | global stepping control |
| utils | logging, timers, provenance, assertions | none or immutable strings | utility outputs | owning domain state |

## Concrete file list for Phase-I implementation skeleton
- Build/bootstrap:
  - `CMakeLists.txt`
  - `tests/CMakeLists.txt`
- Core runtime contracts:
  - `include/cosmosim/core/soa_layout.hpp`
  - `include/cosmosim/core/sim_state.hpp`
  - `src/core/soa_layout.cpp`
  - `src/core/sim_state.cpp`
- Runtime + provenance:
  - `include/cosmosim/runtime/scheduler_cpu.hpp`
  - `src/runtime/scheduler_cpu.cpp`
  - `include/cosmosim/utils/provenance.hpp`
  - `src/utils/provenance.cpp`
  - `src/main.cpp`
- Tests:
  - `tests/unit/test_soa_layout.cpp`

## Rationale for major boundaries (memory locality + maintainability)
- SoA in `core` ensures contiguous streams for solver loops and vectorization; avoids AoS stride penalties.
- Hot/cold split avoids polluting cache lines with IDs/provenance fields not used every step.
- Sidecar discipline localizes optional physics memory growth to activated modules; base memory footprint remains predictable.
- Runtime orchestrator calls module facades instead of cross-linking kernels, preserving debuggability and unit-test seams.
- I/O isolated from solvers prevents schema churn from destabilizing compute kernels and preserves frozen HDF5 names.

## Test plan and diagnostics for Phase-I
- Unit: SoA field-length validation (`test_soa_layout`).
- Integration (next): deterministic 1-step drift with fixed seed and expected checksum.
- Regression (next): compare manifest/provenance tags and snapshot headers across platforms.
- Performance (next): single-node particle sweep throughput and cache-miss counters baseline.

## Documentation updates required
- Add naming conventions doc (variables, files, outputs).
- Add HDF5 frozen field map and compatibility policy.
- Add reproducibility/provenance manifest schema.
