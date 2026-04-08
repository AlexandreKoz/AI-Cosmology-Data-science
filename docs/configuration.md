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


## Analysis and diagnostics keys

The `analysis` section controls standard in-situ diagnostics with explicit cadence and retention:
- `enable_diagnostics` (`true`/`false`)
- `run_health_interval_steps` (> 0): cadence for cheap health counters.
- `science_light_interval_steps` (> 0): cadence for light science products (SF history, angular momentum budgets, quicklook slice/projection).
- `science_heavy_interval_steps` (> 0): cadence for heavy products (currently power spectrum).
- `retention_bundle_count` (>= 1): maximum number of diagnostics bundles retained per run output directory.
- `power_spectrum_mesh_n` (>= 4) and `power_spectrum_bin_count` (>= 1): periodic density-grid and shell binning controls.
- `sf_history_bin_count` (>= 1): star-formation history bin count in scale-factor space.
- `quicklook_grid_n` (>= 4): XY slice/projection quicklook resolution.
- `diagnostics_stem` (`[a-zA-Z0-9_-]`): stable diagnostics bundle filename stem.

Diagnostics bundles record explicit frame and unit conventions (`comoving`, `code`), and write to `<output.output_directory>/<output.run_name>/diagnostics` with stable step-based names.

### Halo/subhalo/merger-tree planning keys (v1 scaffold)

The halo workflow is intentionally explicit and conservative in v1:
- `enable_halo_workflow` (`true`/`false`): enables halo catalog + merger-tree plan products.
- `halo_on_the_fly` (`true`/`false`): ownership mode flag for in-situ vs post-processing orchestration.
- `halo_catalog_stem` (`[a-zA-Z0-9_-]`): stable halo catalog filename stem.
- `merger_tree_stem` (`[a-zA-Z0-9_-]`): stable merger-tree plan filename stem.
- `halo_fof_linking_length_factor` in `(0, 1]`: FOF linking length as a multiple of mean inter-particle spacing.
- `halo_fof_min_group_size` (>= 2): minimum group size retained in catalog output.
- `halo_include_gas`, `halo_include_stars`, `halo_include_black_holes`: species-inclusion toggles; tracers are always excluded.

Current v1 notes:
- FOF is an O(N²) baseline for correctness and interface stabilization, not final scaling behavior.
- Subhalo and merger-tree products are explicit planning scaffolds with schema and provenance, not fully validated production pipelines.

## Cooling/heating keys

The `physics` section also supports cooling/heating normalization keys:
- `cooling_model` (`primordial` or `primordial_plus_metal_table`)
- `uv_background_model` (`hm12`, `fg20`, `none`)
- `self_shielding_model` (`none`, `rahmati13_like`)
- `metal_line_table_path` (optional table asset path)
- `temperature_floor_k` (> 0)

## Stellar feedback keys

Feedback policy is explicit and auditable through typed keys in the `physics` section:
- `fb_mode`: `thermal`, `kinetic`, `momentum`, or `thermal_kinetic_momentum`
- `fb_variant`: `none`, `delayed_cooling`, or `stochastic`
- `fb_use_returned_mass_budget`: if true, use returned-mass budget; if false, use birth-mass proxy budget
- `fb_epsilon_thermal`, `fb_epsilon_kinetic`, `fb_epsilon_momentum` (all >= 0)
- `fb_sn_energy_erg_per_mass_code` (> 0)
- `fb_momentum_code_per_mass_code` (>= 0)
- `fb_neighbor_count` (> 0)
- `fb_delayed_cooling_time_code` (non-negative policy field, recorded in normalized config)
- `fb_stochastic_event_probability` in `(0, 1]`
- `fb_random_seed` (deterministic stochastic-variant seed)

## Tracer keys

Tracer support is optional and gated by both build feature and config:
- Build-time: `COSMOSIM_ENABLE_TRACERS=ON`
- Runtime: `physics.enable_tracers=true`

Typed keys:
- `enable_tracers` (`true`/`false`)
- `tracer_track_mass` (`true`/`false`): if true, tracer particle mass follows host-cell mass with fixed `mass_fraction_of_host`.
- `tracer_min_host_mass_code` (>= 0): conservative guard to skip updates for nearly empty hosts.
