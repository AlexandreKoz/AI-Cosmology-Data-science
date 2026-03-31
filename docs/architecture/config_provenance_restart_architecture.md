# CosmoSim Configuration, Provenance, and Restart Architecture (Phase-I)

## 0) Scope and governing constraints

This architecture follows the project source hierarchy in strict order:

1. Master design: CPU-first, OpenMP baseline, SoA + hot/cold split, sidecar optional physics, deterministic mode and provenance in every run artifact.
2. Variable naming conventions: snake_case, explicit unit suffixes (`_code`, `_comoving_mpc`, `_peculiar_kms`, `_si`).
3. File naming conventions: run root, checkpoint, manifest naming.
4. Existing project decisions: static core library, optional compile-time OpenMP/provenance flags, deterministic slot mapping in state.

No new physical equations are introduced in this document; the design is infrastructure-only.

---


## Deliverables in this rerun

This design is now accompanied by explicit machine-readable contracts:

- `docs/architecture/runtime_config_schema_v1.yaml`
- `docs/architecture/provenance_schema_v1.json`
- `docs/architecture/restart_checkpoint_format_contract.md`

These files are normative for Phase-I implementation details where applicable.

---

## 1) Runtime configuration schema design

### 1.1 Goals

- Preserve `param.txt` ergonomics (line-oriented key/value and comments).
- Add typed schema validation and deterministic normalization.
- Support profile inheritance (e.g., `baseline_cpu`, `debug_deterministic`, `production_largebox`).
- Emit actionable diagnostics with exact path, line, and expected type/range.

### 1.2 Input formats and canonical model

**Accepted authoring formats:**

- `param.txt` style (primary compatibility surface)
- YAML or JSON (optional, same canonical schema)

**Canonical internal model:** strict typed object tree loaded as

`runtime_config_v1`.

All inputs are converted to canonical typed fields before simulation startup.

### 1.3 `param.txt` compatibility grammar

Supported forms:

- `key = value`
- `key value`
- comments: `# ...` and trailing comments after value
- include directive: `include = path/to/file.param`
- profile selector: `profile = profile_name`

Namespaced keys use dot paths, e.g.:

- `cosmology.omega_m`
- `time_integrator.dt_max_code`
- `io.checkpoint.interval_steps`

### 1.4 Schema sections (v1)

- `run`: `run_name`, `output_root`, `random_seed_u64`, `deterministic_mode`.
- `cosmology`: `h0_code`, `omega_m`, `omega_lambda`, `omega_b`.
- `time_integrator`: `a_start`, `a_end`, `dt_max_code`, `cfl_number`.
- `mesh`: `box_size_comoving_mpc`, `base_nx`, `base_ny`, `base_nz`.
- `physics`: per-module boolean enables and module options.
- `io`: `snapshot.interval_steps`, `checkpoint.interval_steps`, compression policy.
- `restart`: `mode` (`fresh|resume`), `path`, `strict_validation`.
- `performance`: `omp_threads`, `omp_schedule`, affinity hints.
- `debug`: floating-point checks, NaN trap, deterministic reductions.

### 1.5 Type system and validation

- Primitive types: `bool`, `int64`, `uint64`, `float64`, `string`, `enum`.
- Composite types: object, fixed-length tuple, homogeneous list.
- Constraints: min/max, open/closed interval, regex, allowed set, dependency rules.
- Cross-field validation examples:
  - `a_start < a_end`
  - if `restart.mode=resume`, then `restart.path` is required
  - if `deterministic_mode=true`, nondeterministic OpenMP schedule is rejected

### 1.6 Profiles and override precedence

Precedence (highest to lowest):

1. CLI overrides (`--set a.b.c=value`)
2. Main config file
3. Included files
4. Selected profile defaults
5. Built-in defaults

The resolved output is written as normalized used-values artifact (Section 3).

### 1.7 Diagnostics contract

Validation failure object fields:

- `error_code` (stable token)
- `message` (human readable)
- `config_path` (dot path)
- `source_file`, `line`, `column` when available
- `expected`
- `actual`
- `hint`

Errors are fail-fast at startup; no partial run begins on invalid config.

---

## 2) Compile-time feature registry design

### 2.1 Purpose

Runtime provenance must record what executable capabilities exist independent of whether they are enabled in config.

### 2.2 Registry structure

Define immutable compile-time registry entries:

- `feature_key` (stable string, e.g., `openmp`, `provenance`, `hdf5`, `fft`, `cooling_module`)
- `enabled` (`true|false`)
- `build_option` (CMake option name)
- `build_value` (raw CMake cache value)
- `abi_impact` (`none|io_schema|state_layout|numerics`)

### 2.3 Emission points

- Included in `provenance.json`
- Included in checkpoint metadata header
- Printed at startup in compact one-line tag for quick log triage

### 2.4 Compatibility rule

Checkpoint load requires compatibility checks against compile-time registry for all entries marked `abi_impact != none`.

---

## 3) Normalized “used values” output

### 3.1 Artifact

Write `run_manifest.yaml` (existing naming policy) and a machine-oriented
`used_values.json` in run root.

### 3.2 Contents

- Fully materialized config tree after profile/default/include/CLI resolution.
- Each field with:
  - `value`
  - `type`
  - `units` (if numeric physical/code quantity)
  - `source` (`default|profile|file|cli`)
  - `source_location` (file:line when available)
- Unknown keys rejected; therefore output has only schema-known keys.

### 3.3 Deterministic serialization

- UTF-8, LF newlines
- sorted object keys
- fixed float formatting policy
- explicit null handling policy

This guarantees byte-stable used-values output for equivalent runs.

---

## 4) `provenance.json` content

### 4.1 Required top-level keys

- `schema_version` (provenance schema version)
- `cosmosim_version` (semantic version + git describe)
- `git`:
  - `commit_sha`
  - `commit_dirty`
  - `branch`
  - `remote_url` (if available)
- `build`:
  - `compiler_id`, `compiler_version`
  - `cxx_standard`
  - `build_type`
  - `compile_flags`
  - `link_flags`
  - `feature_registry` (Section 2)
- `platform`:
  - `hostname`
  - `os`
  - `arch`
  - `cpu_model`
- `runtime`:
  - `utc_start_iso8601`
  - `mpi_world_size` (reserved for future distributed mode)
  - `omp_threads`
  - `env_subset` (allowlist only)
- `inputs`:
  - hashes for config file(s)
  - profile name
  - command-line arguments
- `used_values_hash_sha256`
- `checkpoint_parent` (if restart resume)

### 4.2 Privacy/security policy

Only allowlisted environment variables are recorded (e.g., `OMP_NUM_THREADS`, `OMP_PROC_BIND`).
No full environment dump.

---

## 5) Restart/checkpoint format contract

### 5.1 File and naming

Keep existing naming policy:

- `chkpt_step_<8-digit>.h5`

### 5.2 Contract layers

1. **File header metadata**
   - checkpoint schema version
   - writer CosmoSim version
   - feature registry snapshot
   - used-values hash
   - step/time metadata (`step_id`, `scale_factor_a`, `hubble_H_code`)

2. **Canonical HDF5 groups**
   - `/meta`
   - `/state/hot/...` (SoA arrays only)
   - `/state/cold/...` (cold metadata)
   - `/state/sidecar/<module>/...` (optional physics sidecars)
   - `/indices/deterministic/...` (slot↔dense mapping)

3. **Frozen dataset names**
   - Canonical names once published; additions must be additive.
   - Renames forbidden without migration bridge.

### 5.3 Data layout requirements

- SoA datasets contiguous per field; never AoS dump of hot path state.
- Optional modules absent if disabled; no sparse null padding in core hot groups.
- Numeric units encoded in dataset attributes (`units = code|comoving_mpc|peculiar_kms|si`).

### 5.4 Restart loading modes

- `strict` (default for production): any incompatible ABI-impact feature mismatch is fatal.
- `compatible` (explicit opt-in): allows non-ABI-impact differences with warning.
- `inspect_only`: read metadata without allocating full state.

---

## 6) Versioning and schema migration policy

### 6.1 Version axes

- `runtime_config_schema_version` (e.g., `1.0.0`)
- `provenance_schema_version`
- `checkpoint_schema_version`
- `cosmosim_engine_version`

### 6.2 Compatibility classes

- **Patch:** additive fields or bugfix constraints; backward compatible.
- **Minor:** additive sections or optional datasets; backward compatible readers required.
- **Major:** breaking change; requires explicit migration tool.

### 6.3 Migration mechanism

Provide offline migration command:

`cosmosim_migrate --from <schema> --to <schema> --input <file> --output <file>`

Migration must emit provenance notes with source/target versions and transformation log.

### 6.4 Policy for frozen outputs

HDF5 canonical dataset names are immutable once frozen by policy.
Schema evolution should prefer additive datasets/attributes over mutation.

---

## 7) Deterministic/debug mode behavior

### 7.1 Deterministic mode (`deterministic_mode=true`)

- Fixed reduction ordering (thread-local accumulation + deterministic combine).
- Stable active-set compaction ordering by persistent identifiers.
- Deterministic slot↔dense tables persisted in checkpoint and restored exactly.
- Random streams seeded from `random_seed_u64` + stable stream IDs.
- Disallow nondeterministic scheduler settings.

### 7.2 Debug mode (`debug.*`)

- Optional NaN/Inf scan at kernel boundaries.
- Range assertions on key conserved quantities (module-defined).
- Unit-tag checks on API boundaries where conversions happen.
- Extra checks must be switchable and report overhead in perf logs.

### 7.3 Reproducibility diagnostics

Each run should emit a reproducibility verdict:

- `bitwise_reproducible` (yes/no)
- `reason_if_no` (e.g., non-deterministic reduction enabled)

---

## 8) Error-handling strategy

### 8.1 Malformed config

- Parse errors: show file/line/column snippet.
- Schema errors: show path + expected/actual + fix hint.
- Unknown keys: fail by default; optional `--allow-unknown` for migration audits only.

### 8.2 Invalid restart

Fatal conditions:

- missing required groups/datasets
- checksum/hash mismatch
- ABI-impact compile feature mismatch in strict mode
- schema version unsupported and no migration path

On fatal error, print:

- checkpoint file path
- failing contract rule ID
- remediation suggestion (migrate/rebuild/change mode)

### 8.3 Version mismatch

- Newer checkpoint than reader: fail with `requires_newer_engine`.
- Older checkpoint: attempt supported migration path; otherwise fail with explicit target version.

### 8.4 Error code taxonomy

Use stable prefixed codes:

- `CFG_*` configuration
- `RST_*` restart/checkpoint
- `VER_*` version/migration
- `PRV_*` provenance emission

Error codes are machine-parseable and documented for CI triage.

---

## 9) Minimal implementation roadmap

1. Add schema descriptors and validation engine (runtime config).
2. Add compile-time feature registry generation from CMake options.
3. Emit `used_values.json`, `run_manifest.yaml`, `provenance.json` at startup.
4. Add checkpoint metadata header and strict compatibility checks.
5. Add migration utility skeleton and versioned readers.

This sequence preserves current executable behavior while progressively hardening reproducibility and restart safety.
