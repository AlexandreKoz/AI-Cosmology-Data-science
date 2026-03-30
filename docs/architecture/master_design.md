# CosmoSim Master Design (Phase-I baseline)

- CPU-first single-node performance is the optimization priority.
- OpenMP is the required shared-memory parallel path.
- GPU portability is a planned backend abstraction stage, not a Phase-I dependency.
- Data model is structure-of-arrays with mandatory hot/cold split.
- Optional physics uses sidecar storage keyed by stable identifiers.
- Reproducibility requires deterministic mode and provenance metadata in every run artifact.
- HDF5 field names are frozen once published; compatibility wrappers must preserve canonical names.
