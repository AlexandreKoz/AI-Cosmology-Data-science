#pragma once

#include "cosmosim/core/soa_layout.hpp"

#include <vector>

namespace cosmosim::core {

struct ParticleSpeciesState {
  HotParticleSoA hot_dm;
  HotStarSoA hot_star;
  HotBhSoA hot_bh;

  ColdEntitySidecar cold_dm;
  ColdEntitySidecar cold_star;
  ColdEntitySidecar cold_bh;

  OptionalModuleSidecars sidecar_dm;
  OptionalModuleSidecars sidecar_star;
  OptionalModuleSidecars sidecar_bh;
};

struct CellSpeciesState {
  HotGasCellSoA hot_gas;
  ColdEntitySidecar cold_gas;
  OptionalModuleSidecars sidecar_gas;
};

struct DeterministicIndexState {
  std::vector<std::uint32_t> slot_to_dense_dm;
  std::vector<std::uint32_t> slot_to_dense_gas;
  std::vector<std::uint32_t> slot_to_dense_star;
  std::vector<std::uint32_t> slot_to_dense_bh;

  std::vector<std::uint32_t> dense_to_slot_dm;
  std::vector<std::uint32_t> dense_to_slot_gas;
  std::vector<std::uint32_t> dense_to_slot_star;
  std::vector<std::uint32_t> dense_to_slot_bh;
};

struct SimState {
  ParticleSpeciesState particle_species;
  CellSpeciesState cell_species;
  ActiveSetIndex active_set_index;
  DeterministicIndexState deterministic_index_state;

  double scale_factor_a = 1.0;
  double hubble_H_code = 0.0;
  std::uint64_t step_id = 0;
};

[[nodiscard]] GravityKernelView make_gravity_kernel_view(const SimState& sim_state);
[[nodiscard]] HydroKernelView make_hydro_kernel_view(const SimState& sim_state);
[[nodiscard]] AmrKernelView make_amr_kernel_view(const SimState& sim_state);
[[nodiscard]] IoSnapshotView make_io_snapshot_view_dm(const SimState& sim_state);
[[nodiscard]] AnalysisView make_analysis_view_dm(const SimState& sim_state);

} // namespace cosmosim::core
