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
