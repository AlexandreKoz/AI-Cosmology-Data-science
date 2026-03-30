#include "cosmosim/core/sim_state.hpp"
#include "cosmosim/core/soa_layout.hpp"

#include <cassert>

int main() {
  cosmosim::core::HotParticleSoA particles{};
  particles.x_comoving_mpc = {0.0F, 1.0F};
  particles.y_comoving_mpc = {0.0F, 1.0F};
  particles.z_comoving_mpc = {0.0F, 1.0F};
  particles.vx_peculiar_kms = {0.0F, 0.0F};
  particles.vy_peculiar_kms = {0.0F, 0.0F};
  particles.vz_peculiar_kms = {0.0F, 0.0F};
  particles.mass_code = {1.0F, 1.0F};
  particles.eps_grav_comoving_mpc = {0.01F, 0.01F};

  assert(cosmosim::core::validate_hot_layout(particles));
  assert(cosmosim::core::active_particle_count(particles) == 2);

  particles.mass_code.pop_back();
  assert(!cosmosim::core::validate_hot_layout(particles));

  cosmosim::core::ColdEntitySidecar cold{};
  cold.persistent_id = {1U, 2U};
  cold.restart_slot = {0U, 1U};
  cold.io_group_code = {0U, 0U};
  cold.provenance_flags = {0U, 0U};
  assert(cosmosim::core::validate_cold_layout(cold, 2));

  const cosmosim::core::EntityByteBudget dm_budget = cosmosim::core::dm_byte_budget();
  assert(dm_budget.total_bytes_baseline > 0);
  assert(dm_budget.total_bytes_flagship >= dm_budget.total_bytes_baseline);

  cosmosim::core::SimState sim_state{};
  sim_state.particle_species.hot_dm = particles;
  sim_state.particle_species.cold_dm = cold;
  sim_state.active_set_index.particle_dm_index = {0U};

  const cosmosim::core::GravityKernelView gravity_view = cosmosim::core::make_gravity_kernel_view(sim_state);
  assert(gravity_view.particle_dm_index.size() == 1);
  assert(gravity_view.x_comoving_mpc.size() == 2);

  const cosmosim::core::AnalysisView analysis_view = cosmosim::core::make_analysis_view_dm(sim_state);
  assert(analysis_view.persistent_id.size() == 2);

  return 0;
}
