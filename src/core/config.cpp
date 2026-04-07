#include "cosmosim/core/config.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

#include "cosmosim/core/provenance.hpp"
#include "cosmosim/core/simulation_mode.hpp"

namespace cosmosim::core {
namespace {

struct ParsedEntry {
  std::string value;
  int line_number = 0;
};

[[nodiscard]] std::string trim(const std::string& input) {
  const auto begin = std::find_if_not(input.begin(), input.end(), [](unsigned char c) {
    return std::isspace(c) != 0;
  });
  const auto end = std::find_if_not(input.rbegin(), input.rend(), [](unsigned char c) {
    return std::isspace(c) != 0;
  }).base();

  if (begin >= end) {
    return {};
  }

  return std::string(begin, end);
}

[[nodiscard]] std::string toLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

[[nodiscard]] std::string removeInlineComment(const std::string& line) {
  std::size_t comment_pos = std::string::npos;
  for (const std::string marker : {"#", ";", "//"}) {
    const std::size_t pos = line.find(marker);
    if (pos != std::string::npos && (comment_pos == std::string::npos || pos < comment_pos)) {
      comment_pos = pos;
    }
  }

  if (comment_pos == std::string::npos) {
    return line;
  }
  return line.substr(0, comment_pos);
}

[[nodiscard]] std::map<std::string, ParsedEntry> parseEntries(const std::string& text) {
  std::istringstream stream(text);
  std::string raw_line;
  int line_number = 0;
  std::string current_section;
  std::map<std::string, ParsedEntry> entries;

  while (std::getline(stream, raw_line)) {
    ++line_number;
    const std::string line = trim(removeInlineComment(raw_line));
    if (line.empty()) {
      continue;
    }

    if (line.front() == '[' && line.back() == ']') {
      current_section = toLower(trim(line.substr(1, line.size() - 2)));
      if (current_section.empty()) {
        throw ConfigError("line " + std::to_string(line_number) + ": empty section name");
      }
      continue;
    }

    std::string key;
    std::string value;
    const std::size_t equal_pos = line.find('=');
    if (equal_pos != std::string::npos) {
      key = trim(line.substr(0, equal_pos));
      value = trim(line.substr(equal_pos + 1));
    } else {
      std::istringstream token_stream(line);
      token_stream >> key;
      std::getline(token_stream, value);
      value = trim(value);
    }

    key = toLower(trim(key));
    if (key.empty() || value.empty()) {
      throw ConfigError("line " + std::to_string(line_number) + ": expected key and value");
    }

    if (!current_section.empty() && key.find('.') == std::string::npos) {
      key = current_section + "." + key;
    }

    auto [it, inserted] = entries.emplace(key, ParsedEntry{value, line_number});
    if (!inserted) {
      throw ConfigError("line " + std::to_string(line_number) + ": duplicate key '" + key +
                        "' (first at line " + std::to_string(it->second.line_number) + ")");
    }
  }

  return entries;
}

[[nodiscard]] bool parseBool(const std::string& value, const std::string& key) {
  const std::string lower = toLower(trim(value));
  if (lower == "1" || lower == "true" || lower == "on" || lower == "yes") {
    return true;
  }
  if (lower == "0" || lower == "false" || lower == "off" || lower == "no") {
    return false;
  }
  throw ConfigError("key '" + key + "': invalid boolean value '" + value + "'");
}

template <typename T>
[[nodiscard]] T parseNumber(const std::string& value, const std::string& key) {
  const std::string trimmed = trim(value);
  T number{};
  const char* begin = trimmed.data();
  const char* end = trimmed.data() + trimmed.size();
  const auto [ptr, ec] = std::from_chars(begin, end, number);
  if (ec != std::errc{} || ptr != end) {
    throw ConfigError("key '" + key + "': invalid numeric value '" + value + "'");
  }
  return number;
}

[[nodiscard]] double parseFloating(const std::string& value, const std::string& key) {
  const std::string trimmed = trim(value);
  char* consumed = nullptr;
  const double number = std::strtod(trimmed.c_str(), &consumed);
  if (consumed != trimmed.c_str() + static_cast<std::ptrdiff_t>(trimmed.size())) {
    throw ConfigError("key '" + key + "': invalid floating-point value '" + value + "'");
  }
  return number;
}

[[nodiscard]] std::pair<double, std::string> splitMagnitudeUnit(const std::string& value) {
  std::istringstream stream(value);
  double magnitude = 0.0;
  stream >> magnitude;
  if (stream.fail()) {
    throw ConfigError("invalid value '" + value + "': expected numeric magnitude");
  }

  std::string unit;
  stream >> unit;
  unit = toLower(unit);
  return {magnitude, unit};
}

[[nodiscard]] double lengthToMpc(double value, const std::string& unit) {
  const std::string normalized = toLower(unit);
  if (normalized.empty() || normalized == "mpc") {
    return value;
  }
  if (normalized == "kpc") {
    return value * 1.0e-3;
  }
  throw ConfigError("unsupported length unit '" + unit + "' (supported: mpc, kpc)");
}

[[nodiscard]] double parseLengthMpc(
    const std::string& value,
    const std::string& default_unit,
    const std::string& key) {
  const auto [magnitude, provided_unit] = splitMagnitudeUnit(value);
  const std::string unit = provided_unit.empty() ? default_unit : provided_unit;
  try {
    return lengthToMpc(magnitude, unit);
  } catch (const ConfigError&) {
    throw ConfigError("key '" + key + "': unsupported unit in value '" + value + "'");
  }
}

[[nodiscard]] double parseLengthKpc(
    const std::string& value,
    const std::string& default_unit,
    const std::string& key) {
  const double in_mpc = parseLengthMpc(value, default_unit, key);
  return in_mpc * 1.0e3;
}

[[nodiscard]] std::string requireString(
    std::map<std::string, ParsedEntry>& entries,
    std::set<std::string>& consumed,
    const std::string& key,
    const std::string& default_value) {
  const auto it = entries.find(key);
  if (it == entries.end()) {
    return default_value;
  }
  consumed.insert(key);
  return trim(it->second.value);
}

[[nodiscard]] std::string sanitizeStem(const std::string& value, const std::string& key) {
  if (value.empty()) {
    throw ConfigError("key '" + key + "': value cannot be empty");
  }
  for (const char c : value) {
    const bool valid = std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-';
    if (!valid) {
      throw ConfigError(
          "key '" + key + "': only [a-zA-Z0-9_-] are allowed for stable output naming");
    }
  }
  return value;
}

[[nodiscard]] std::string modeToLowerString(SimulationMode mode) {
  switch (mode) {
    case SimulationMode::kCosmoCube:
      return "cosmo_cube";
    case SimulationMode::kZoomIn:
      return "zoom_in";
    case SimulationMode::kIsolatedGalaxy:
      return "isolated_galaxy";
    case SimulationMode::kIsolatedCluster:
      return "isolated_cluster";
  }
  return "unknown";
}

[[nodiscard]] SimulationMode parseMode(const std::string& value) {
  const std::string lower = toLower(trim(value));
  if (lower == "cosmo_cube") {
    return SimulationMode::kCosmoCube;
  }
  if (lower == "zoom_in") {
    return SimulationMode::kZoomIn;
  }
  if (lower == "isolated_galaxy") {
    return SimulationMode::kIsolatedGalaxy;
  }
  if (lower == "isolated_cluster") {
    return SimulationMode::kIsolatedCluster;
  }
  throw ConfigError("key 'mode.mode': invalid mode '" + value + "'");
}

void validateConfig(const SimulationConfig& config) {
  if (config.schema_version != 1) {
    throw ConfigError("schema_version must be 1 for this build");
  }
  if (config.numerics.time_end_code <= config.numerics.time_begin_code) {
    throw ConfigError("numerics.time_end_code must be greater than numerics.time_begin_code");
  }
  if (config.numerics.max_global_steps <= 0) {
    throw ConfigError("numerics.max_global_steps must be > 0");
  }
  if (config.output.snapshot_interval_steps <= 0) {
    throw ConfigError("output.snapshot_interval_steps must be > 0");
  }
  if (config.parallel.mpi_ranks_expected <= 0 || config.parallel.omp_threads <= 0) {
    throw ConfigError("parallel settings require positive mpi_ranks_expected and omp_threads");
  }
  if (config.cosmology.omega_matter <= 0.0 || config.cosmology.omega_lambda < 0.0) {
    throw ConfigError("cosmology requires omega_matter > 0 and omega_lambda >= 0");
  }
  if (config.physics.temperature_floor_k <= 0.0) {
    throw ConfigError("physics.temperature_floor_k must be > 0");
  }
  if (config.physics.star_formation_epsilon_ff < 0.0 || config.physics.star_formation_epsilon_ff > 1.0) {
    throw ConfigError("physics.star_formation_epsilon_ff must be in [0, 1]");
  }
  if (config.physics.star_formation_density_threshold_code <= 0.0) {
    throw ConfigError("physics.star_formation_density_threshold_code must be > 0");
  }
  if (config.physics.star_formation_temperature_threshold_k <= 0.0) {
    throw ConfigError("physics.star_formation_temperature_threshold_k must be > 0");
  }
  if (config.physics.star_formation_gravitational_constant_code <= 0.0) {
    throw ConfigError("physics.star_formation_gravitational_constant_code must be > 0");
  }
  if (config.physics.star_formation_minimum_particle_mass_code <= 0.0) {
    throw ConfigError("physics.star_formation_minimum_particle_mass_code must be > 0");
  }
  const std::string spawn_mode = toLower(config.physics.star_formation_spawn_mode);
  if (spawn_mode != "deterministic" && spawn_mode != "stochastic") {
    throw ConfigError("physics.star_formation_spawn_mode must be deterministic or stochastic");
  }
  const ModePolicy policy = buildModePolicy(config.mode);
  validateModePolicy(config, policy);
}


[[nodiscard]] std::string buildNormalizedText(const FrozenConfig& frozen) {
  std::ostringstream stream;
  stream << "schema_version = " << frozen.config.schema_version << '\n';
  stream << "provenance.config_hash = " << frozen.provenance.config_hash_hex << '\n';
  stream << "\n[units]\n";
  stream << "length_unit = " << frozen.config.units.length_unit << '\n';
  stream << "mass_unit = " << frozen.config.units.mass_unit << '\n';
  stream << "velocity_unit = " << frozen.config.units.velocity_unit << '\n';
  stream << "coordinate_frame = " << frozen.config.units.coordinate_frame << '\n';
  stream << "\n[mode]\n";
  stream << "mode = " << modeToLowerString(frozen.config.mode.mode) << '\n';
  stream << "ic_file = " << frozen.config.mode.ic_file << '\n';
  stream << "zoom_high_res_region = " << (frozen.config.mode.zoom_high_res_region ? "true" : "false")
         << '\n';
  stream << "zoom_region_file = " << frozen.config.mode.zoom_region_file << '\n';
  stream << "hydro_boundary = " << frozen.config.mode.hydro_boundary << '\n';
  stream << "gravity_boundary = " << frozen.config.mode.gravity_boundary << '\n';
  stream << "\n[cosmology]\n";
  stream << "omega_matter = " << frozen.config.cosmology.omega_matter << '\n';
  stream << "omega_lambda = " << frozen.config.cosmology.omega_lambda << '\n';
  stream << "omega_baryon = " << frozen.config.cosmology.omega_baryon << '\n';
  stream << "hubble_param = " << frozen.config.cosmology.hubble_param << '\n';
  stream << "sigma8 = " << frozen.config.cosmology.sigma8 << '\n';
  stream << "scalar_index_ns = " << frozen.config.cosmology.scalar_index_ns << '\n';
  stream << "box_size_mpc_comoving = " << frozen.config.cosmology.box_size_mpc_comoving << '\n';
  stream << "\n[numerics]\n";
  stream << "time_begin_code = " << frozen.config.numerics.time_begin_code << '\n';
  stream << "time_end_code = " << frozen.config.numerics.time_end_code << '\n';
  stream << "max_global_steps = " << frozen.config.numerics.max_global_steps << '\n';
  stream << "hierarchical_max_rung = " << frozen.config.numerics.hierarchical_max_rung << '\n';
  stream << "amr_max_level = " << frozen.config.numerics.amr_max_level << '\n';
  stream << "gravity_softening_kpc_comoving = "
         << frozen.config.numerics.gravity_softening_kpc_comoving << '\n';
  stream << "gravity_solver = " << frozen.config.numerics.gravity_solver << '\n';
  stream << "hydro_solver = " << frozen.config.numerics.hydro_solver << '\n';
  stream << "\n[physics]\n";
  stream << "enable_cooling = " << (frozen.config.physics.enable_cooling ? "true" : "false") << '\n';
  stream << "enable_star_formation = "
         << (frozen.config.physics.enable_star_formation ? "true" : "false") << '\n';
  stream << "enable_feedback = " << (frozen.config.physics.enable_feedback ? "true" : "false") << '\n';
  stream << "reionization_model = " << frozen.config.physics.reionization_model << '\n';
  stream << "uv_background_model = " << frozen.config.physics.uv_background_model << '\n';
  stream << "self_shielding_model = " << frozen.config.physics.self_shielding_model << '\n';
  stream << "cooling_model = " << frozen.config.physics.cooling_model << '\n';
  stream << "metal_line_table_path = " << frozen.config.physics.metal_line_table_path << '\n';
  stream << "temperature_floor_k = " << frozen.config.physics.temperature_floor_k << '\n';
  stream << "\n[output]\n";
  stream << "run_name = " << frozen.config.output.run_name << '\n';
  stream << "output_directory = " << frozen.config.output.output_directory << '\n';
  stream << "output_stem = " << frozen.config.output.output_stem << '\n';
  stream << "restart_stem = " << frozen.config.output.restart_stem << '\n';
  stream << "snapshot_interval_steps = " << frozen.config.output.snapshot_interval_steps << '\n';
  stream << "write_restarts = " << (frozen.config.output.write_restarts ? "true" : "false") << '\n';
  stream << "\n[parallel]\n";
  stream << "mpi_ranks_expected = " << frozen.config.parallel.mpi_ranks_expected << '\n';
  stream << "omp_threads = " << frozen.config.parallel.omp_threads << '\n';
  stream << "gpu_devices = " << frozen.config.parallel.gpu_devices << '\n';
  stream << "deterministic_reduction = "
         << (frozen.config.parallel.deterministic_reduction ? "true" : "false") << '\n';
  stream << "\n[compatibility]\n";
  stream << "allow_unknown_keys = "
         << (frozen.config.compatibility.allow_unknown_keys ? "true" : "false") << '\n';
  return stream.str();
}

[[nodiscard]] FrozenConfig normalizeValidateFreeze(
    const std::map<std::string, ParsedEntry>& parsed_entries,
    const std::string& source_name,
    const ParseOptions& options) {
  std::map<std::string, ParsedEntry> entries = parsed_entries;
  std::set<std::string> consumed;
  FrozenConfig frozen;
  frozen.provenance.source_name = source_name;

  std::vector<std::pair<std::string, std::string>> deprecated = {
      {"omega0", "cosmology.omega_matter"},
      {"omegalambda", "cosmology.omega_lambda"},
      {"hubbleparam", "cosmology.hubble_param"},
      {"timemax", "numerics.time_end_code"},
      {"mode", "mode.mode"},
      {"run_name", "output.run_name"},
  };

  for (const auto& [legacy_key, canonical_key] : deprecated) {
    const auto it = entries.find(legacy_key);
    if (it == entries.end()) {
      continue;
    }
    if (entries.contains(canonical_key)) {
      throw ConfigError("deprecated key '" + legacy_key + "' cannot be combined with '" +
                        canonical_key + "'");
    }
    entries.emplace(canonical_key, it->second);
    consumed.insert(legacy_key);
    frozen.provenance.deprecation_warnings.push_back(
        "deprecated key '" + legacy_key + "' mapped to '" + canonical_key + "'");
  }

  frozen.config.schema_version = static_cast<int>(parseNumber<long>(
      requireString(entries, consumed, "schema_version", "1"), "schema_version"));

  frozen.config.units.length_unit =
      toLower(requireString(entries, consumed, "units.length_unit", frozen.config.units.length_unit));
  frozen.config.units.mass_unit =
      toLower(requireString(entries, consumed, "units.mass_unit", frozen.config.units.mass_unit));
  frozen.config.units.velocity_unit = toLower(
      requireString(entries, consumed, "units.velocity_unit", frozen.config.units.velocity_unit));
  frozen.config.units.coordinate_frame = toLower(
      requireString(entries, consumed, "units.coordinate_frame", frozen.config.units.coordinate_frame));

  frozen.config.mode.mode = parseMode(requireString(entries, consumed, "mode.mode", "zoom_in"));
  frozen.config.mode.ic_file = requireString(entries, consumed, "mode.ic_file", "ics.hdf5");
  frozen.config.mode.zoom_high_res_region = parseBool(
      requireString(entries, consumed, "mode.zoom_high_res_region", "false"),
      "mode.zoom_high_res_region");
  frozen.config.mode.zoom_region_file =
      requireString(entries, consumed, "mode.zoom_region_file", "");
  frozen.config.mode.hydro_boundary = toLower(
      requireString(entries, consumed, "mode.hydro_boundary", frozen.config.mode.hydro_boundary));
  frozen.config.mode.gravity_boundary = toLower(
      requireString(entries, consumed, "mode.gravity_boundary", frozen.config.mode.gravity_boundary));

  frozen.config.cosmology.omega_matter = parseFloating(
      requireString(entries, consumed, "cosmology.omega_matter", "0.315"),
      "cosmology.omega_matter");
  frozen.config.cosmology.omega_lambda = parseFloating(
      requireString(entries, consumed, "cosmology.omega_lambda", "0.685"),
      "cosmology.omega_lambda");
  frozen.config.cosmology.omega_baryon = parseFloating(
      requireString(entries, consumed, "cosmology.omega_baryon", "0.049"),
      "cosmology.omega_baryon");
  frozen.config.cosmology.hubble_param = parseFloating(
      requireString(entries, consumed, "cosmology.hubble_param", "0.674"),
      "cosmology.hubble_param");
  frozen.config.cosmology.sigma8 =
      parseFloating(requireString(entries, consumed, "cosmology.sigma8", "0.811"), "cosmology.sigma8");
  frozen.config.cosmology.scalar_index_ns = parseFloating(
      requireString(entries, consumed, "cosmology.scalar_index_ns", "0.965"),
      "cosmology.scalar_index_ns");
  frozen.config.cosmology.box_size_mpc_comoving = parseLengthMpc(
      requireString(entries, consumed, "cosmology.box_size", "50.0"),
      frozen.config.units.length_unit,
      "cosmology.box_size");

  frozen.config.numerics.time_begin_code = parseFloating(
      requireString(entries, consumed, "numerics.time_begin_code", "0.0"),
      "numerics.time_begin_code");
  frozen.config.numerics.time_end_code = parseFloating(
      requireString(entries, consumed, "numerics.time_end_code", "1.0"),
      "numerics.time_end_code");
  frozen.config.numerics.max_global_steps = static_cast<int>(parseNumber<long>(
      requireString(entries, consumed, "numerics.max_global_steps", "1024"),
      "numerics.max_global_steps"));
  frozen.config.numerics.hierarchical_max_rung = static_cast<int>(parseNumber<long>(
      requireString(entries, consumed, "numerics.hierarchical_max_rung", "12"),
      "numerics.hierarchical_max_rung"));
  frozen.config.numerics.amr_max_level = static_cast<int>(parseNumber<long>(
      requireString(entries, consumed, "numerics.amr_max_level", "10"), "numerics.amr_max_level"));
  frozen.config.numerics.gravity_softening_kpc_comoving = parseLengthKpc(
      requireString(entries, consumed, "numerics.gravity_softening", "1.0 kpc"),
      frozen.config.units.length_unit,
      "numerics.gravity_softening");
  frozen.config.numerics.gravity_solver =
      requireString(entries, consumed, "numerics.gravity_solver", "treepm");
  frozen.config.numerics.hydro_solver =
      requireString(entries, consumed, "numerics.hydro_solver", "godunov_fv");

  frozen.config.physics.enable_cooling = parseBool(
      requireString(entries, consumed, "physics.enable_cooling", "true"), "physics.enable_cooling");
  frozen.config.physics.enable_star_formation =
      parseBool(requireString(entries, consumed, "physics.enable_star_formation", "true"),
                "physics.enable_star_formation");
  frozen.config.physics.enable_feedback = parseBool(
      requireString(entries, consumed, "physics.enable_feedback", "true"), "physics.enable_feedback");
  frozen.config.physics.reionization_model =
      requireString(entries, consumed, "physics.reionization_model", "hm12");
  frozen.config.physics.uv_background_model =
      requireString(entries, consumed, "physics.uv_background_model", "hm12");
  frozen.config.physics.self_shielding_model =
      requireString(entries, consumed, "physics.self_shielding_model", "none");
  frozen.config.physics.cooling_model =
      requireString(entries, consumed, "physics.cooling_model", "primordial");
  frozen.config.physics.metal_line_table_path =
      requireString(entries, consumed, "physics.metal_line_table_path", "");
  frozen.config.physics.temperature_floor_k = parseFloating(
      requireString(entries, consumed, "physics.temperature_floor_k", "100.0"),
      "physics.temperature_floor_k");
  frozen.config.physics.star_formation_epsilon_ff = parseFloating(
      requireString(entries, consumed, "physics.star_formation_epsilon_ff", "0.01"),
      "physics.star_formation_epsilon_ff");
  frozen.config.physics.star_formation_density_threshold_code = parseFloating(
      requireString(entries, consumed, "physics.star_formation_density_threshold_code", "10.0"),
      "physics.star_formation_density_threshold_code");
  frozen.config.physics.star_formation_temperature_threshold_k = parseFloating(
      requireString(entries, consumed, "physics.star_formation_temperature_threshold_k", "10000.0"),
      "physics.star_formation_temperature_threshold_k");
  frozen.config.physics.star_formation_max_velocity_divergence_code = parseFloating(
      requireString(entries, consumed, "physics.star_formation_max_velocity_divergence_code", "0.0"),
      "physics.star_formation_max_velocity_divergence_code");
  frozen.config.physics.star_formation_gravitational_constant_code = parseFloating(
      requireString(entries, consumed, "physics.star_formation_gravitational_constant_code", "1.0"),
      "physics.star_formation_gravitational_constant_code");
  frozen.config.physics.star_formation_minimum_particle_mass_code = parseFloating(
      requireString(entries, consumed, "physics.star_formation_minimum_particle_mass_code", "0.01"),
      "physics.star_formation_minimum_particle_mass_code");
  frozen.config.physics.star_formation_spawn_mode = toLower(requireString(
      entries,
      consumed,
      "physics.star_formation_spawn_mode",
      "stochastic"));
  frozen.config.physics.star_formation_rng_seed = static_cast<std::uint64_t>(parseNumber<unsigned long long>(
      requireString(entries, consumed, "physics.star_formation_rng_seed", "1"),
      "physics.star_formation_rng_seed"));

  frozen.config.output.run_name =
      requireString(entries, consumed, "output.run_name", frozen.config.output.run_name);
  frozen.config.output.output_directory =
      requireString(entries, consumed, "output.output_directory", frozen.config.output.output_directory);
  frozen.config.output.output_stem = sanitizeStem(
      requireString(entries, consumed, "output.output_stem", frozen.config.output.output_stem),
      "output.output_stem");
  frozen.config.output.restart_stem = sanitizeStem(
      requireString(entries, consumed, "output.restart_stem", frozen.config.output.restart_stem),
      "output.restart_stem");
  frozen.config.output.snapshot_interval_steps = static_cast<int>(parseNumber<long>(
      requireString(entries, consumed, "output.snapshot_interval_steps", "64"),
      "output.snapshot_interval_steps"));
  frozen.config.output.write_restarts = parseBool(
      requireString(entries, consumed, "output.write_restarts", "true"), "output.write_restarts");

  frozen.config.parallel.mpi_ranks_expected = static_cast<int>(parseNumber<long>(
      requireString(entries, consumed, "parallel.mpi_ranks_expected", "1"),
      "parallel.mpi_ranks_expected"));
  frozen.config.parallel.omp_threads = static_cast<int>(parseNumber<long>(
      requireString(entries, consumed, "parallel.omp_threads", "1"), "parallel.omp_threads"));
  frozen.config.parallel.gpu_devices = static_cast<int>(parseNumber<long>(
      requireString(entries, consumed, "parallel.gpu_devices", "0"), "parallel.gpu_devices"));
  frozen.config.parallel.deterministic_reduction =
      parseBool(requireString(entries, consumed, "parallel.deterministic_reduction", "true"),
                "parallel.deterministic_reduction");

  const bool compatible_by_file = parseBool(
      requireString(entries, consumed, "compatibility.allow_unknown_keys", "false"),
      "compatibility.allow_unknown_keys");
  frozen.config.compatibility.allow_unknown_keys = options.allow_unknown_keys || compatible_by_file;

  validateConfig(frozen.config);

  std::vector<std::string> unknown;
  for (const auto& [key, _] : entries) {
    if (!consumed.contains(key)) {
      unknown.push_back(key);
    }
  }

  if (!unknown.empty() && !frozen.config.compatibility.allow_unknown_keys) {
    std::ostringstream stream;
    stream << "unknown parameter keys:";
    for (const std::string& key : unknown) {
      stream << ' ' << key;
    }
    stream << " (set compatibility.allow_unknown_keys=true to bypass)";
    throw ConfigError(stream.str());
  }

  frozen.provenance.config_hash = 0;
  frozen.provenance.config_hash_hex = "0000000000000000";
  frozen.normalized_text = buildNormalizedText(frozen);

  frozen.provenance.config_hash = stableConfigHash(frozen.normalized_text);
  frozen.provenance.config_hash_hex = stableConfigHashHex(frozen.normalized_text);
  frozen.normalized_text = buildNormalizedText(frozen);

  return frozen;
}

}  // namespace

ConfigError::ConfigError(const std::string& message) : std::runtime_error(message) {}

FrozenConfig loadFrozenConfigFromFile(const std::filesystem::path& path, const ParseOptions& options) {
  std::ifstream input(path);
  if (!input) {
    throw ConfigError("failed to open config file: " + path.string());
  }
  std::ostringstream contents;
  contents << input.rdbuf();
  return loadFrozenConfigFromString(contents.str(), path.string(), options);
}

FrozenConfig loadFrozenConfigFromString(
    const std::string& config_text,
    const std::string& source_name,
    const ParseOptions& options) {
  const auto entries = parseEntries(config_text);
  return normalizeValidateFreeze(entries, source_name, options);
}

void writeNormalizedConfigSnapshot(
    const FrozenConfig& frozen_config,
    const std::filesystem::path& run_directory) {
  std::filesystem::create_directories(run_directory);
  const std::filesystem::path snapshot_path = run_directory / "normalized_config.param.txt";
  std::ofstream output(snapshot_path);
  if (!output) {
    throw ConfigError("failed to write normalized config snapshot: " + snapshot_path.string());
  }
  output << "# normalized_config generated by cosmosim\n";
  output << "# source = " << frozen_config.provenance.source_name << '\n';
  output << frozen_config.normalized_text;
}

std::string modeToString(SimulationMode mode) {
  return modeToLowerString(mode);
}

}  // namespace cosmosim::core
