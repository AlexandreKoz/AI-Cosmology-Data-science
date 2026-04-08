#pragma once

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace cosmosim::core {

enum class SimulationMode {
  kCosmoCube,
  kZoomIn,
  kIsolatedGalaxy,
  kIsolatedCluster,
};

struct CosmologyConfig {
  double omega_matter = 0.315;
  double omega_lambda = 0.685;
  double omega_baryon = 0.049;
  double hubble_param = 0.674;
  double sigma8 = 0.811;
  double scalar_index_ns = 0.965;
  double box_size_mpc_comoving = 50.0;
};

struct NumericsConfig {
  double time_begin_code = 0.0;
  double time_end_code = 1.0;
  int max_global_steps = 1024;
  int hierarchical_max_rung = 12;
  int amr_max_level = 10;
  double gravity_softening_kpc_comoving = 1.0;
  std::string gravity_solver = "treepm";
  std::string hydro_solver = "godunov_fv";
};

struct PhysicsConfig {
  bool enable_cooling = true;
  bool enable_star_formation = true;
  bool enable_feedback = true;
  bool enable_stellar_evolution = true;
  std::string reionization_model = "hm12";
  std::string uv_background_model = "hm12";
  std::string self_shielding_model = "none";
  std::string cooling_model = "primordial";
  std::string metal_line_table_path;
  double temperature_floor_k = 100.0;
  double sf_density_threshold_code = 10.0;
  double sf_temperature_threshold_k = 1.0e4;
  double sf_min_converging_flow_rate_code = 0.0;
  double sf_epsilon_ff = 0.01;
  double sf_min_star_particle_mass_code = 0.1;
  bool sf_stochastic_spawning = true;
  std::uint64_t sf_random_seed = 123456789ull;
  std::string fb_mode = "thermal_kinetic_momentum";
  std::string fb_variant = "none";
  bool fb_use_returned_mass_budget = true;
  double fb_epsilon_thermal = 0.6;
  double fb_epsilon_kinetic = 0.3;
  double fb_epsilon_momentum = 0.1;
  double fb_sn_energy_erg_per_mass_code = 1.0e49;
  double fb_momentum_code_per_mass_code = 3.0e3;
  std::uint32_t fb_neighbor_count = 8;
  double fb_delayed_cooling_time_code = 0.0;
  double fb_stochastic_event_probability = 0.25;
  std::uint64_t fb_random_seed = 42424242ull;
  std::string stellar_evolution_table_path;
  double stellar_evolution_hubble_time_years = 1.44e10;
  bool enable_black_hole_agn = false;
  double bh_seed_halo_mass_threshold_code = 1.0e3;
  double bh_seed_mass_code = 1.0;
  std::uint32_t bh_seed_max_per_cell = 1;
  double bh_alpha_bondi = 1.0;
  bool bh_use_eddington_cap = true;
  double bh_epsilon_r = 0.1;
  double bh_epsilon_f = 0.05;
  double bh_feedback_coupling_efficiency = 1.0;
  double bh_duty_cycle_active_edd_ratio_threshold = 0.01;
  double bh_proton_mass_si = 1.67262192369e-27;
  double bh_thomson_cross_section_si = 6.6524587321e-29;
  double bh_newton_g_si = 6.67430e-11;
  double bh_speed_of_light_si = 2.99792458e8;
  bool enable_tracers = false;
  bool tracer_track_mass = true;
  double tracer_min_host_mass_code = 0.0;
};

struct OutputConfig {
  std::string run_name = "cosmosim_run";
  std::string output_directory = "outputs";
  std::string output_stem = "snapshot";
  std::string restart_stem = "restart";
  int snapshot_interval_steps = 64;
  bool write_restarts = true;
};

struct ParallelConfig {
  int mpi_ranks_expected = 1;
  int omp_threads = 1;
  int gpu_devices = 0;
  bool deterministic_reduction = true;
};

struct UnitsConfig {
  std::string length_unit = "mpc";
  std::string mass_unit = "msun";
  std::string velocity_unit = "km_s";
  std::string coordinate_frame = "comoving";
};

struct ModeConfig {
  SimulationMode mode = SimulationMode::kZoomIn;
  std::string ic_file = "ics.hdf5";
  bool zoom_high_res_region = false;
  std::string zoom_region_file;
  std::string hydro_boundary = "auto";
  std::string gravity_boundary = "auto";
};

struct CompatibilityConfig {
  bool allow_unknown_keys = false;
};

struct SimulationConfig {
  int schema_version = 1;
  CosmologyConfig cosmology;
  NumericsConfig numerics;
  PhysicsConfig physics;
  OutputConfig output;
  ParallelConfig parallel;
  UnitsConfig units;
  ModeConfig mode;
  CompatibilityConfig compatibility;
};

struct ProvenanceMetadata {
  std::string source_name;
  std::uint64_t config_hash = 0;
  std::string config_hash_hex;
  std::vector<std::string> deprecation_warnings;
};

struct FrozenConfig {
  SimulationConfig config;
  std::string normalized_text;
  ProvenanceMetadata provenance;
};

struct ParseOptions {
  bool allow_unknown_keys = false;
};

class ConfigError : public std::runtime_error {
 public:
  explicit ConfigError(const std::string& message);
};

[[nodiscard]] FrozenConfig loadFrozenConfigFromFile(
    const std::filesystem::path& path,
    const ParseOptions& options = {});

[[nodiscard]] FrozenConfig loadFrozenConfigFromString(
    const std::string& config_text,
    const std::string& source_name,
    const ParseOptions& options = {});

void writeNormalizedConfigSnapshot(
    const FrozenConfig& frozen_config,
    const std::filesystem::path& run_directory);

[[nodiscard]] std::string modeToString(SimulationMode mode);

}  // namespace cosmosim::core
