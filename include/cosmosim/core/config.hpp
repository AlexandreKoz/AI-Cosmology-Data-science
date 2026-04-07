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
