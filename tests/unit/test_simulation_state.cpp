#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>

#include "cosmosim/core/simulation_state.hpp"

int main() {
  cosmosim::core::SimulationState state;

  state.metadata.schema_version = 4;
  state.metadata.run_name = "zoom_in_test";
  state.metadata.config_digest = "sha256:deadbeef";
  state.metadata.initial_conditions_source = "ic_zoom.hdf5";
  state.metadata.provenance_stamp = "git:abc123";
  state.metadata.normalized_config_text = "box_size=40.0";

  const auto dump = state.metadata.normalizedDump();
  assert(dump.find("schema_version=4") != std::string::npos);
  assert(dump.find("run_name=zoom_in_test") != std::string::npos);

  auto& dm = state.speciesState(cosmosim::core::ParticleSpecies::dark_matter);
  dm.resize(8);
  assert(dm.size() == 8);
  assert(dm.validateOwnershipInvariant());

  auto& gas = state.speciesState(cosmosim::core::ParticleSpecies::gas);
  gas.resize(5);
  assert(gas.size() == 5);

  state.cells.resize(6);
  state.patches.resize(3);
  assert(state.validateOwnershipInvariants());

  constexpr std::uint32_t active_index_data[4] = {1, 3, 4, 7};
  state.active_particle_local_indices.assign(active_index_data);
  assert(state.active_particle_local_indices.view().size() == 4);

  auto& cooling_sidecar = state.ensureModuleSidecar("cooling", sizeof(double));
  cooling_sidecar.resizeElements(16);
  assert(cooling_sidecar.elementCount() == 16);
  assert(state.findModuleSidecar("cooling") != nullptr);

  cosmosim::core::StepWorkspace workspace;
  auto scratch = workspace.allocator->acquire(128, alignof(std::uint64_t));
  assert(scratch.size() == 128);
  workspace.gather_map = {0, 2, 4};
  workspace.staging_scalar = {1.0, 2.0};
  workspace.clearForNextStep();
  assert(workspace.gather_map.empty());
  assert(workspace.staging_scalar.empty());

  return 0;
}
