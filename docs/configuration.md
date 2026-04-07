# Configuration system (param-style workflow)

CosmoSim accepts a line-oriented `param.txt` style workflow and normalizes it into a typed validated configuration object before any rank-divergent execution.

## Workflow

1. **Parse** user config (`key=value` or `key value`, comments with `#`, `;`, or `//`, optional sections).
2. **Normalize** values into typed fields under these groups:
   - `cosmology`
   - `numerics`
   - `physics`
   - `output`
   - `parallel`
   - `units`
   - `mode`
3. **Validate** schema and semantic constraints (ranges, enum values, stable naming, mode-specific requirements).
4. **Freeze** deterministic normalized output and FNV-1a provenance hash.

## Reproducibility and provenance

- Unknown keys fail by default.
- Compatibility mode exists only when `compatibility.allow_unknown_keys=true` (or parse option override).
- Deprecated keys are mapped with explicit warnings captured in provenance metadata.
- Canonical normalized config can be emitted to `<run_directory>/normalized_config.param.txt`.

## Stable names

`output.output_stem` and `output.restart_stem` accept only `[a-zA-Z0-9_-]` to preserve stable naming across snapshots, restarts, tests, and benchmarks.


## Simulation modes and boundaries

`mode.mode` selects one of `cosmo_cube`, `zoom_in`, `isolated_galaxy`, or `isolated_cluster`.

Boundary behavior is explicit and normalized into the frozen config:
- `mode.hydro_boundary`: `auto`, `periodic`, `open`, or `reflective`.
- `mode.gravity_boundary`: `auto`, `periodic`, or `isolated_monopole`.

Mode-policy validation rules:
- `cosmo_cube` and `zoom_in` require periodic hydro and periodic Poisson gravity.
- `isolated_galaxy` and `isolated_cluster` require non-periodic gravity (`isolated_monopole`), with hydro boundary selected by policy or explicit override.
- `zoom_in` with `mode.zoom_high_res_region=true` still requires `mode.zoom_region_file`.

For isolated gravity, the current boundary treatment is a monopole/Dirichlet-style reference potential ghost fill (`isolated_monopole`) documented in the mode policy and preserved in normalized snapshots for provenance.

## Cooling/heating keys

The `physics` section also supports cooling/heating normalization keys:
- `cooling_model` (`primordial` or `primordial_plus_metal_table`)
- `uv_background_model` (`hm12`, `fg20`, `none`)
- `self_shielding_model` (`none`, `rahmati13_like`)
- `metal_line_table_path` (optional table asset path)
- `temperature_floor_k` (> 0)
