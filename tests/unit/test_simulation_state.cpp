#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include "cosmosim/core/simulation_state.hpp"

int main() {
  cosmosim::core::SimulationState state;
  state.resizeParticles(4);
  state.resizeCells(3);
  state.resizePatches(1);

  for (std::size_t i = 0; i < 4; ++i) {
    state.particle_sidecar.particle_id[i] = 1000 + i;
    state.particle_sidecar.species_tag[i] =
        static_cast<std::uint32_t>(cosmosim::core::ParticleSpecies::kDarkMatter);
    state.particles.position_x_comoving[i] = static_cast<double>(i);
    state.particles.position_y_comoving[i] = static_cast<double>(i + 1);
    state.particles.position_z_comoving[i] = static_cast<double>(i + 2);
    state.particles.velocity_x_peculiar[i] = 0.1;
    state.particles.velocity_y_peculiar[i] = 0.2;
    state.particles.velocity_z_peculiar[i] = 0.3;
    state.particles.mass_code[i] = 1.0;
    state.particles.internal_energy_code[i] = 0.0;
  }
  state.species.count_by_species = {4, 0, 0, 0, 0};

  state.patches.patch_id[0] = 42;
  state.patches.level[0] = 0;
  state.patches.first_cell[0] = 0;
  state.patches.cell_count[0] = 3;

  for (std::size_t i = 0; i < 3; ++i) {
    state.cells.density_code[i] = 10.0 + static_cast<double>(i);
    state.cells.pressure_code[i] = 1.0 + static_cast<double>(i);
    state.cells.velocity_x_peculiar[i] = 0.01;
    state.cells.velocity_y_peculiar[i] = 0.02;
    state.cells.velocity_z_peculiar[i] = 0.03;
    state.cells.patch_index[i] = 0;
  }

  assert(state.validateOwnershipInvariants());

  state.metadata.run_name = "unit_state";
  state.metadata.normalized_config_hash = 1234;
  state.metadata.normalized_config_hash_hex = "0x4d2";
  state.metadata.step_index = 17;
  state.metadata.scale_factor = 0.5;

  const std::string serialized = state.metadata.serialize();
  const auto parsed = cosmosim::core::StateMetadata::deserialize(serialized);
  assert(parsed.run_name == state.metadata.run_name);
  assert(parsed.normalized_config_hash == state.metadata.normalized_config_hash);
  assert(parsed.step_index == state.metadata.step_index);
  assert(parsed.scale_factor == state.metadata.scale_factor);

  cosmosim::core::ModuleSidecarRegistry registry;
  cosmosim::core::ModuleSidecarBlock block;
  block.module_name = "cooling";
  block.schema_version = 2;
  block.payload = {std::byte{0xAA}, std::byte{0xBB}};
  registry.upsert(block);
  assert(registry.size() == 1);
  const auto* found = registry.find("cooling");
  assert(found != nullptr);
  assert(found->schema_version == 2);
  assert(found->payload.size() == 2);

  cosmosim::core::TransientStepWorkspace workspace;
  const std::array<std::uint32_t, 2> particle_indices{1, 3};
  auto particle_view = cosmosim::core::buildParticleActiveView(state, particle_indices, workspace);
  assert(particle_view.size() == 2);
  assert(particle_view.particle_id[0] == 1001);
  assert(particle_view.position_x_comoving[1] == 3.0);

  const std::array<std::uint32_t, 1> cell_indices{2};
  auto cell_view = cosmosim::core::buildCellActiveView(state, cell_indices, workspace);
  assert(cell_view.size() == 1);
  assert(cell_view.density_code[0] == 12.0);

  bool threw = false;
  try {
    const std::array<std::uint32_t, 1> bad_indices{8};
    (void)cosmosim::core::buildParticleActiveView(state, bad_indices, workspace);
  } catch (const std::out_of_range&) {
    threw = true;
  }
  assert(threw);

  return 0;
}
