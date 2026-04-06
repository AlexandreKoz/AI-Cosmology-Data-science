#include "cosmosim/core/simulation_state.hpp"

#include <charconv>
#include <cstdlib>
#include <limits>
#include <new>
#include <sstream>

namespace cosmosim::core {
namespace {

// Species tag validity helper for sidecar-to-ledger consistency checks.
[[nodiscard]] bool isValidSpeciesTag(std::uint32_t value) {
  return value <= static_cast<std::uint32_t>(ParticleSpecies::kTracer);
}

[[nodiscard]] std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\n\r");
  if (first == std::string::npos) {
    return {};
  }
  const auto last = value.find_last_not_of(" \t\n\r");
  return value.substr(first, last - first + 1);
}

[[nodiscard]] std::uint64_t parseUint64(std::string_view value, const std::string& key) {
  std::uint64_t parsed = 0;
  const auto* begin = value.data();
  const auto* end = value.data() + value.size();
  const auto [ptr, ec] = std::from_chars(begin, end, parsed);
  if (ec != std::errc{} || ptr != end) {
    throw std::invalid_argument("StateMetadata.deserialize: invalid integer for key '" + key + "'");
  }
  return parsed;
}

[[nodiscard]] std::uint32_t parseUint32(std::string_view value, const std::string& key) {
  const auto parsed = parseUint64(value, key);
  if (parsed > std::numeric_limits<std::uint32_t>::max()) {
    throw std::invalid_argument("StateMetadata.deserialize: integer overflow for key '" + key + "'");
  }
  return static_cast<std::uint32_t>(parsed);
}

[[nodiscard]] double parseDouble(std::string_view value, const std::string& key) {
  std::string temp(value);
  char* end = nullptr;
  const double parsed = std::strtod(temp.c_str(), &end);
  if (end != temp.c_str() + static_cast<std::ptrdiff_t>(temp.size())) {
    throw std::invalid_argument(
        "StateMetadata.deserialize: invalid floating-point value for key '" + key + "'");
  }
  return parsed;
}

}  // namespace

void ParticleSoa::resize(std::size_t count) {
  // Keep all hot arrays in lock-step to preserve contiguous index ownership.
  position_x_comoving.resize(count);
  position_y_comoving.resize(count);
  position_z_comoving.resize(count);
  velocity_x_peculiar.resize(count);
  velocity_y_peculiar.resize(count);
  velocity_z_peculiar.resize(count);
  mass_code.resize(count);
  internal_energy_code.resize(count);
}

std::size_t ParticleSoa::size() const noexcept { return position_x_comoving.size(); }

bool ParticleSoa::isConsistent() const noexcept {
  const std::size_t expected = position_x_comoving.size();
  return position_y_comoving.size() == expected && position_z_comoving.size() == expected &&
         velocity_x_peculiar.size() == expected && velocity_y_peculiar.size() == expected &&
         velocity_z_peculiar.size() == expected && mass_code.size() == expected &&
         internal_energy_code.size() == expected;
}

void ParticleSidecar::resize(std::size_t count) {
  // Sidecar arrays share the same particle index space as ParticleSoa.
  particle_id.resize(count);
  species_tag.resize(count);
  particle_flags.resize(count);
  owning_rank.resize(count);
}

std::size_t ParticleSidecar::size() const noexcept { return particle_id.size(); }

bool ParticleSidecar::isConsistent() const noexcept {
  const std::size_t expected = particle_id.size();
  return species_tag.size() == expected && particle_flags.size() == expected &&
         owning_rank.size() == expected;
}

void CellSoa::resize(std::size_t count) {
  // Cell arrays remain lock-step for contiguous gather/scatter patterns.
  density_code.resize(count);
  pressure_code.resize(count);
  velocity_x_peculiar.resize(count);
  velocity_y_peculiar.resize(count);
  velocity_z_peculiar.resize(count);
  patch_index.resize(count);
}

std::size_t CellSoa::size() const noexcept { return density_code.size(); }

bool CellSoa::isConsistent() const noexcept {
  const std::size_t expected = density_code.size();
  return pressure_code.size() == expected && velocity_x_peculiar.size() == expected &&
         velocity_y_peculiar.size() == expected && velocity_z_peculiar.size() == expected &&
         patch_index.size() == expected;
}

void PatchSoa::resize(std::size_t count) {
  // Patch descriptors are stored in compact SoA form for traversal locality.
  patch_id.resize(count);
  level.resize(count);
  first_cell.resize(count);
  cell_count.resize(count);
}

std::size_t PatchSoa::size() const noexcept { return patch_id.size(); }

bool PatchSoa::isConsistent() const noexcept {
  const std::size_t expected = patch_id.size();
  return level.size() == expected && first_cell.size() == expected && cell_count.size() == expected;
}

std::uint64_t SpeciesContainer::totalCount() const noexcept {
  std::uint64_t total = 0;
  for (const auto count : count_by_species) {
    total += count;
  }
  return total;
}

bool SpeciesContainer::isConsistentWith(const ParticleSidecar& sidecar) const noexcept {
  // Recompute measured species counts from sidecar tags and compare to ledger.
  std::array<std::uint64_t, 5> measured{};
  for (const auto tag : sidecar.species_tag) {
    if (!isValidSpeciesTag(tag)) {
      return false;
    }
    ++measured.at(tag);
  }
  return measured == count_by_species;
}

std::string StateMetadata::serialize() const {
  // Stable key order intentionally supports deterministic snapshots/restarts.
  std::ostringstream out;
  out << "schema_version=" << schema_version << '\n';
  out << "run_name=" << run_name << '\n';
  out << "normalized_config_hash=" << normalized_config_hash << '\n';
  out << "normalized_config_hash_hex=" << normalized_config_hash_hex << '\n';
  out << "step_index=" << step_index << '\n';
  out << "scale_factor=" << scale_factor << '\n';
  out << "snapshot_stem=" << snapshot_stem << '\n';
  out << "restart_stem=" << restart_stem << '\n';
  return out.str();
}

StateMetadata StateMetadata::deserialize(std::string_view text) {
  StateMetadata metadata;
  std::istringstream in{std::string(text)};
  std::string line;

  while (std::getline(in, line)) {
    const std::string trimmed = trim(line);
    if (trimmed.empty()) {
      continue;
    }

    const auto equal_pos = trimmed.find('=');
    if (equal_pos == std::string::npos || equal_pos == 0 || equal_pos == trimmed.size() - 1) {
      throw std::invalid_argument("StateMetadata.deserialize: malformed line '" + line + "'");
    }

    const std::string key = trim(trimmed.substr(0, equal_pos));
    const std::string value = trim(trimmed.substr(equal_pos + 1));

    if (key == "schema_version") {
      metadata.schema_version = parseUint32(value, key);
    } else if (key == "run_name") {
      metadata.run_name = value;
    } else if (key == "normalized_config_hash") {
      metadata.normalized_config_hash = parseUint64(value, key);
    } else if (key == "normalized_config_hash_hex") {
      metadata.normalized_config_hash_hex = value;
    } else if (key == "step_index") {
      metadata.step_index = parseUint64(value, key);
    } else if (key == "scale_factor") {
      metadata.scale_factor = parseDouble(value, key);
    } else if (key == "snapshot_stem") {
      metadata.snapshot_stem = value;
    } else if (key == "restart_stem") {
      metadata.restart_stem = value;
    }
  }

  return metadata;
}

void ModuleSidecarRegistry::upsert(ModuleSidecarBlock block) {
  if (block.module_name.empty()) {
    throw std::invalid_argument("ModuleSidecarRegistry.upsert: module_name cannot be empty");
  }
  m_sidecars[block.module_name] = std::move(block);
}

const ModuleSidecarBlock* ModuleSidecarRegistry::find(std::string_view module_name) const {
  const auto it = m_sidecars.find(std::string(module_name));
  if (it == m_sidecars.end()) {
    return nullptr;
  }
  return &it->second;
}

std::size_t ModuleSidecarRegistry::size() const noexcept { return m_sidecars.size(); }

void SimulationState::resizeParticles(std::size_t count) {
  particles.resize(count);
  particle_sidecar.resize(count);
}

void SimulationState::resizeCells(std::size_t count) { cells.resize(count); }

void SimulationState::resizePatches(std::size_t count) { patches.resize(count); }

bool SimulationState::validateOwnershipInvariants() const {
  // Structural consistency of each SoA block.
  if (!particles.isConsistent() || !particle_sidecar.isConsistent() || !cells.isConsistent() ||
      !patches.isConsistent()) {
    return false;
  }

  // Particle hot/cold arrays must share one ownership cardinality.
  if (particles.size() != particle_sidecar.size()) {
    return false;
  }

  // Species ledger must match sidecar tags exactly.
  if (!species.isConsistentWith(particle_sidecar)) {
    return false;
  }

  // Every cell must reference a valid patch index when patches exist.
  for (std::size_t i = 0; i < cells.patch_index.size(); ++i) {
    if (cells.patch_index[i] >= patches.size() && patches.size() > 0) {
      return false;
    }
  }

  // Every patch-declared cell range must fall inside the global cell index space.
  for (std::size_t patch = 0; patch < patches.size(); ++patch) {
    const std::uint64_t begin = patches.first_cell[patch];
    const std::uint64_t count = patches.cell_count[patch];
    if (begin + count > cells.size()) {
      return false;
    }
  }

  return true;
}

void ActiveIndexSet::clear() {
  particle_indices.clear();
  cell_indices.clear();
}

std::size_t ParticleActiveView::size() const noexcept { return particle_id.size(); }

std::size_t CellActiveView::size() const noexcept { return density_code.size(); }

MonotonicScratchAllocator::MonotonicScratchAllocator(std::size_t initial_capacity_bytes)
    : m_storage(initial_capacity_bytes), m_offset_bytes(0) {}

std::byte* MonotonicScratchAllocator::allocateBytes(std::size_t bytes, std::size_t alignment) {
  // Require power-of-two alignment to preserve standard aligned-address arithmetic.
  if (alignment == 0 || (alignment & (alignment - 1U)) != 0) {
    throw std::invalid_argument("MonotonicScratchAllocator.allocateBytes: alignment must be power-of-two");
  }

  if (bytes == 0) {
    return m_storage.data() + m_offset_bytes;
  }

  // Round offset upward to the requested alignment boundary.
  const std::size_t aligned_offset = (m_offset_bytes + alignment - 1U) & ~(alignment - 1U);
  const std::size_t required_size = aligned_offset + bytes;

  // Geometric growth to keep amortized allocation overhead low.
  if (required_size > m_storage.size()) {
    const std::size_t grow_size = std::max(required_size, std::max<std::size_t>(1024, m_storage.size() * 2));
    m_storage.resize(grow_size);
  }

  auto* ptr = m_storage.data() + aligned_offset;
  m_offset_bytes = required_size;
  return ptr;
}

void MonotonicScratchAllocator::reset() { m_offset_bytes = 0; }

std::size_t MonotonicScratchAllocator::capacityBytes() const noexcept { return m_storage.size(); }

void TransientStepWorkspace::clear() {
  // Preserve capacity while dropping active step data.
  particle_id.clear();
  particle_species_tag.clear();
  particle_position_x_comoving.clear();
  particle_position_y_comoving.clear();
  particle_position_z_comoving.clear();
  particle_velocity_x_peculiar.clear();
  particle_velocity_y_peculiar.clear();
  particle_velocity_z_peculiar.clear();
  particle_mass_code.clear();

  cell_density_code.clear();
  cell_pressure_code.clear();
  cell_velocity_x_peculiar.clear();
  cell_velocity_y_peculiar.clear();
  cell_velocity_z_peculiar.clear();
  cell_patch_index.clear();

  scratch.reset();
}

ParticleActiveView buildParticleActiveView(
    const SimulationState& state,
    std::span<const std::uint32_t> active_particle_indices,
    TransientStepWorkspace& workspace) {
  // Materialize compact particle arrays for kernel-friendly contiguous iteration.
  workspace.particle_id.resize(active_particle_indices.size());
  workspace.particle_species_tag.resize(active_particle_indices.size());
  workspace.particle_position_x_comoving.resize(active_particle_indices.size());
  workspace.particle_position_y_comoving.resize(active_particle_indices.size());
  workspace.particle_position_z_comoving.resize(active_particle_indices.size());
  workspace.particle_velocity_x_peculiar.resize(active_particle_indices.size());
  workspace.particle_velocity_y_peculiar.resize(active_particle_indices.size());
  workspace.particle_velocity_z_peculiar.resize(active_particle_indices.size());
  workspace.particle_mass_code.resize(active_particle_indices.size());

  for (std::size_t i = 0; i < active_particle_indices.size(); ++i) {
    const std::uint32_t source = active_particle_indices[i];
    if (source >= state.particles.size()) {
      throw std::out_of_range("buildParticleActiveView: particle index out of range");
    }

    workspace.particle_id[i] = state.particle_sidecar.particle_id[source];
    workspace.particle_species_tag[i] = state.particle_sidecar.species_tag[source];
    workspace.particle_position_x_comoving[i] = state.particles.position_x_comoving[source];
    workspace.particle_position_y_comoving[i] = state.particles.position_y_comoving[source];
    workspace.particle_position_z_comoving[i] = state.particles.position_z_comoving[source];
    workspace.particle_velocity_x_peculiar[i] = state.particles.velocity_x_peculiar[source];
    workspace.particle_velocity_y_peculiar[i] = state.particles.velocity_y_peculiar[source];
    workspace.particle_velocity_z_peculiar[i] = state.particles.velocity_z_peculiar[source];
    workspace.particle_mass_code[i] = state.particles.mass_code[source];
  }

  return ParticleActiveView{
      .particle_id = workspace.particle_id,
      .species_tag = workspace.particle_species_tag,
      .position_x_comoving = workspace.particle_position_x_comoving,
      .position_y_comoving = workspace.particle_position_y_comoving,
      .position_z_comoving = workspace.particle_position_z_comoving,
      .velocity_x_peculiar = workspace.particle_velocity_x_peculiar,
      .velocity_y_peculiar = workspace.particle_velocity_y_peculiar,
      .velocity_z_peculiar = workspace.particle_velocity_z_peculiar,
      .mass_code = workspace.particle_mass_code,
  };
}

CellActiveView buildCellActiveView(
    const SimulationState& state,
    std::span<const std::uint32_t> active_cell_indices,
    TransientStepWorkspace& workspace) {
  // Materialize compact cell arrays for kernel-friendly contiguous iteration.
  workspace.cell_density_code.resize(active_cell_indices.size());
  workspace.cell_pressure_code.resize(active_cell_indices.size());
  workspace.cell_velocity_x_peculiar.resize(active_cell_indices.size());
  workspace.cell_velocity_y_peculiar.resize(active_cell_indices.size());
  workspace.cell_velocity_z_peculiar.resize(active_cell_indices.size());
  workspace.cell_patch_index.resize(active_cell_indices.size());

  for (std::size_t i = 0; i < active_cell_indices.size(); ++i) {
    const std::uint32_t source = active_cell_indices[i];
    if (source >= state.cells.size()) {
      throw std::out_of_range("buildCellActiveView: cell index out of range");
    }

    workspace.cell_density_code[i] = state.cells.density_code[source];
    workspace.cell_pressure_code[i] = state.cells.pressure_code[source];
    workspace.cell_velocity_x_peculiar[i] = state.cells.velocity_x_peculiar[source];
    workspace.cell_velocity_y_peculiar[i] = state.cells.velocity_y_peculiar[source];
    workspace.cell_velocity_z_peculiar[i] = state.cells.velocity_z_peculiar[source];
    workspace.cell_patch_index[i] = state.cells.patch_index[source];
  }

  return CellActiveView{
      .density_code = workspace.cell_density_code,
      .pressure_code = workspace.cell_pressure_code,
      .velocity_x_peculiar = workspace.cell_velocity_x_peculiar,
      .velocity_y_peculiar = workspace.cell_velocity_y_peculiar,
      .velocity_z_peculiar = workspace.cell_velocity_z_peculiar,
      .patch_index = workspace.cell_patch_index,
  };
}

}  // namespace cosmosim::core
