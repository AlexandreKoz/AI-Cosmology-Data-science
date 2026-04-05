#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

#include "cosmosim/core/simulation_state.hpp"

namespace {

void runNoOpPipelineStep(cosmosim::core::SimulationState& state,
                         cosmosim::core::StepWorkspace& workspace) {
  const auto active_particles = state.active_particle_local_indices.view();
  auto& dm = state.speciesState(cosmosim::core::ParticleSpecies::dark_matter);

  workspace.gather_map.assign(active_particles.begin(), active_particles.end());
  workspace.staging_scalar.resize(active_particles.size());

  for (std::size_t i = 0; i < active_particles.size(); ++i) {
    const auto local_index = static_cast<std::size_t>(active_particles[i]);
    workspace.staging_scalar[i] = dm.hot.mass_code[local_index];
  }

  auto* diagnostics = state.findModuleSidecar("pipeline_diag");
  assert(diagnostics != nullptr);
  diagnostics->resizeElements(active_particles.size());

  workspace.clearForNextStep();
  state.clearTransientViews();
}

}  // namespace

int main() {
  cosmosim::core::SimulationState state;
  cosmosim::core::StepWorkspace workspace;

  auto& dm = state.speciesState(cosmosim::core::ParticleSpecies::dark_matter);
  dm.resize(32);
  std::fill(dm.hot.mass_code.begin(), dm.hot.mass_code.end(), 1.0);

  const std::vector<std::uint32_t> active = {0, 2, 5, 9, 11};
  state.active_particle_local_indices.assign(active);

  auto& diag = state.ensureModuleSidecar("pipeline_diag", sizeof(std::uint32_t));
  assert(diag.element_bytes == sizeof(std::uint32_t));

  runNoOpPipelineStep(state, workspace);

  assert(state.active_particle_local_indices.view().empty());
  assert(workspace.gather_map.empty());
  assert(workspace.staging_scalar.empty());
  assert(diag.elementCount() == active.size());
  assert(state.validateOwnershipInvariants());

  return 0;
}
