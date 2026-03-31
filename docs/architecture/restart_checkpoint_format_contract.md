# CosmoSim Restart/Checkpoint Format Contract (v1)

## 1) File naming and placement

- Run root: `runs/run_<utc_yyyymmddThhmmssZ>_<short_gitsha>/`
- Checkpoint file: `chkpt_step_<8-digit>.h5`
- Associated metadata files in run root:
  - `run_manifest.yaml`
  - `used_values.json`
  - `provenance.json`

## 2) HDF5 layout (canonical)

```text
/meta
  checkpoint_schema_version          (attribute or scalar dataset)
  writer_cosmosim_version            (string)
  writer_git_commit_sha              (string)
  compile_feature_registry_json      (string)
  used_values_hash_sha256            (string)
  step_id                            (uint64)
  scale_factor_a                     (float64)
  hubble_H_code                      (float64)
  crc64_state                        (uint64)

/state/hot/dm
  x_comoving_mpc                     (float64[N])
  y_comoving_mpc                     (float64[N])
  z_comoving_mpc                     (float64[N])
  vx_peculiar_kms                    (float64[N])
  vy_peculiar_kms                    (float64[N])
  vz_peculiar_kms                    (float64[N])
  mass_code                          (float64[N])

/state/cold/dm
  persistent_id                      (uint64[N])
  restart_slot                       (uint32[N])

/state/sidecar/<module>/<species>
  ... module-owned optional datasets ...

/indices/deterministic
  slot_to_dense_dm                   (uint32[S])
  dense_to_slot_dm                   (uint32[N])
  slot_to_dense_gas                  (uint32[Sg])
  dense_to_slot_gas                  (uint32[Ng])
  ...
```

## 3) Dataset naming and evolution policy

- Canonical dataset names are frozen once published in a released checkpoint schema.
- Breaking rename/removal is forbidden for the same major schema version.
- Additive datasets/attributes are allowed for minor versions.
- If rename is unavoidable at major bump, migration tooling must create backward-compatible aliasing behavior.

## 4) Units and semantic tagging

Each numeric dataset with physical meaning must carry attributes:

- `units`: one of `code`, `comoving_mpc`, `peculiar_kms`, `si`
- `semantic_role`: one of `state_hot`, `state_cold`, `index_map`, `sidecar`

Unit conversion is not performed during checkpoint read/write.

## 5) Restart validation contract

Validation classes:

1. **Structural**: required groups/datasets exist and expected rank/dtype match.
2. **Semantic**: required attributes (`units`, schema versions) present.
3. **Compatibility**: compile feature registry ABI-impact entries match current executable.
4. **Integrity**: state checksum/hash fields pass.

Restart modes:

- `strict`: all validation classes must pass.
- `compatible`: allows ABI-neutral differences with warning records.
- `inspect_only`: metadata-only load, no state allocation.

## 6) Failure reporting contract

On failure, emit a structured report:

- `error_code` (`RST_*`)
- `checkpoint_path`
- `schema_version_found`
- `schema_version_expected`
- `failing_rule_id`
- `message`
- `hint`

No partial state load is committed when validation fails.
