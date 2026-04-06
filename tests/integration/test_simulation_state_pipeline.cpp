#include <cassert>
#include <cstddef>
#include <cstdint>

#include "cosmosim/cosmosim.hpp"

int main() {
  cosmosim::core::SimulationState state;
  state.metadata.run_name = "integration_pipeline";
  state.resizeParticles(8);
  state.resizeCells(4);
  state.resizePatches(1);

  state.species.count_by_species = {4, 4, 0, 0, 0};

  for (std::size_t i = 0; i < 8; ++i) {
    state.particle_sidecar.particle_id[i] = 100000 + i;
    state.particle_sidecar.species_tag[i] = (i < 4) ? 0U : 1U;
    state.particles.position_x_comoving[i] = static_cast<double>(i);
    state.particles.position_y_comoving[i] = static_cast<double>(i) * 2.0;
    state.particles.position_z_comoving[i] = static_cast<double>(i) * 3.0;
    state.particles.velocity_x_peculiar[i] = 0.0;
    state.particles.velocity_y_peculiar[i] = 0.0;
    state.particles.velocity_z_peculiar[i] = 0.0;
    state.particles.mass_code[i] = 1.0;
    state.particles.internal_energy_code[i] = 0.1;
  }

  state.patches.patch_id[0] = 9000;
  state.patches.level[0] = 1;
  state.patches.first_cell[0] = 0;
  state.patches.cell_count[0] = 4;

  for (std::size_t i = 0; i < 4; ++i) {
    state.cells.density_code[i] = 1.0 + static_cast<double>(i);
    state.cells.pressure_code[i] = 2.0 + static_cast<double>(i);
    state.cells.velocity_x_peculiar[i] = 0.01;
    state.cells.velocity_y_peculiar[i] = 0.02;
    state.cells.velocity_z_peculiar[i] = 0.03;
    state.cells.patch_index[i] = 0;
  }

  assert(state.validateOwnershipInvariants());

  cosmosim::core::ActiveIndexSet active_set;
  active_set.particle_indices = {0, 2, 4, 6};
  active_set.cell_indices = {1, 3};

  cosmosim::core::TransientStepWorkspace workspace;
  const auto particle_view =
      cosmosim::core::buildParticleActiveView(state, active_set.particle_indices, workspace);
  const auto cell_view = cosmosim::core::buildCellActiveView(state, active_set.cell_indices, workspace);

  double checksum = 0.0;
  for (std::size_t i = 0; i < particle_view.size(); ++i) {
    checksum += particle_view.position_x_comoving[i] + particle_view.position_y_comoving[i] +
                particle_view.position_z_comoving[i];
  }
  for (std::size_t i = 0; i < cell_view.size(); ++i) {
    checksum += cell_view.density_code[i] + cell_view.pressure_code[i];
  }

  assert(checksum > 0.0);
  return 0;
}
