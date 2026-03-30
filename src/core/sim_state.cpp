#include "cosmosim/core/sim_state.hpp"

namespace cosmosim::core {

GravityKernelView make_gravity_kernel_view(const SimState& sim_state) {
  return GravityKernelView{
      .particle_dm_index = sim_state.active_set_index.particle_dm_index,
      .x_comoving_mpc = sim_state.particle_species.hot_dm.x_comoving_mpc,
      .y_comoving_mpc = sim_state.particle_species.hot_dm.y_comoving_mpc,
      .z_comoving_mpc = sim_state.particle_species.hot_dm.z_comoving_mpc,
      .mass_code = sim_state.particle_species.hot_dm.mass_code,
  };
}

HydroKernelView make_hydro_kernel_view(const SimState& sim_state) {
  return HydroKernelView{
      .cell_gas_index = sim_state.active_set_index.cell_gas_index,
      .rho_code = sim_state.cell_species.hot_gas.rho_code,
      .u_thermal_code = sim_state.cell_species.hot_gas.u_thermal_code,
      .vx_peculiar_kms = sim_state.cell_species.hot_gas.vx_peculiar_kms,
      .vy_peculiar_kms = sim_state.cell_species.hot_gas.vy_peculiar_kms,
      .vz_peculiar_kms = sim_state.cell_species.hot_gas.vz_peculiar_kms,
  };
}

AmrKernelView make_amr_kernel_view(const SimState& sim_state) {
  return AmrKernelView{
      .cell_gas_index = sim_state.active_set_index.cell_gas_index,
      .rho_code = sim_state.cell_species.hot_gas.rho_code,
      .smoothing_length_comoving_mpc = sim_state.cell_species.hot_gas.smoothing_length_comoving_mpc,
  };
}

IoSnapshotView make_io_snapshot_view_dm(const SimState& sim_state) {
  return IoSnapshotView{
      .persistent_id = sim_state.particle_species.cold_dm.persistent_id,
      .x_comoving_mpc = sim_state.particle_species.hot_dm.x_comoving_mpc,
      .y_comoving_mpc = sim_state.particle_species.hot_dm.y_comoving_mpc,
      .z_comoving_mpc = sim_state.particle_species.hot_dm.z_comoving_mpc,
      .vx_peculiar_kms = sim_state.particle_species.hot_dm.vx_peculiar_kms,
      .vy_peculiar_kms = sim_state.particle_species.hot_dm.vy_peculiar_kms,
      .vz_peculiar_kms = sim_state.particle_species.hot_dm.vz_peculiar_kms,
      .mass_code = sim_state.particle_species.hot_dm.mass_code,
  };
}

AnalysisView make_analysis_view_dm(const SimState& sim_state) {
  return AnalysisView{
      .persistent_id = sim_state.particle_species.cold_dm.persistent_id,
      .restart_slot = sim_state.particle_species.cold_dm.restart_slot,
      .mass_code = sim_state.particle_species.hot_dm.mass_code,
  };
}

} // namespace cosmosim::core
