# CosmoSim Variable Naming Conventions

- Use `snake_case` for variables and function names.
- Include unit suffixes in variable names:
  - `_comoving_mpc`
  - `_peculiar_kms`
  - `_code`
  - `_si` (explicit conversion points only)
- Prefix state ownership context where needed:
  - `hot_` for hot-path containers
  - `cold_` for cold-path metadata
- Avoid ambiguous names (`x`, `v`, `rho`) in public interfaces unless inside tight local loop scope.
