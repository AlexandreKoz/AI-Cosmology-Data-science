# Time Integration Framework

This document defines the baseline time-integration contract for CosmoSim and records conservative assumptions made in the first implementation.

## Stage contract

The authoritative per-step stage order is:

1. `gravity_kick_pre`
2. `drift`
3. `hydro_update`
4. `source_terms`
5. `gravity_kick_post`
6. `analysis_hooks`
7. `output_check`

The scheduler (`StageScheduler`) owns this ordering and solver modules interact via `IntegrationCallback` implementations. This keeps gravity/hydro/source/output sequencing explicit and auditable.

## State and active sets

`IntegratorState` tracks:

- `current_time_code`
- `current_scale_factor`
- `dt_time_code`
- `step_index`
- `TimeBinContext` (hierarchical-bin scaffold)

`ActiveSetDescriptor` carries compact particle/cell subsets and subset flags, avoiding global full-state sweeps in scheduler logic.

## Cosmology helper equations

The helper API is aligned with comoving-variable usage:

- `dx/dt = v_pec / a`
- `dv_pec/dt = -H(a) v_pec - (1/a) grad_x phi + source_terms`
- `da/dt = a H(a)`

The first implementation provides midpoint-integrated drift/kick prefactors over scale factor and a closed-form Hubble drag factor `a_begin / a_end`.

## Assumptions (explicit)

- Baseline stepping scheme is kick-drift-kick only.
- Scale factor update in the orchestrator uses forward Euler (`advanceScaleFactorEuler`) for conservative simplicity.
- If no cosmology background is passed to `StepOrchestrator`, the orchestrator advances `time` only and leaves `current_scale_factor` unchanged.
- Output cadence policy remains external to the core integrator; output triggering is represented by the explicit `output_check` stage callback.

## Provenance and schema implications

No snapshot schema field is changed by this patch. Integrator state lives in runtime objects and can be serialized in a follow-up restart/state I/O patch without changing the stage contract.
