#include <cassert>
#include <filesystem>
#include <stdexcept>
#include <string>

#include "cosmosim/core/build_config.hpp"
#include "cosmosim/core/provenance.hpp"
#include "cosmosim/io/snapshot_hdf5.hpp"

namespace {

void fillMixedSpeciesState(cosmosim::core::SimulationState& state) {
  state.resizeParticles(6);
  for (std::size_t i = 0; i < state.particles.size(); ++i) {
    state.particles.position_x_comoving[i] = static_cast<double>(i) * 0.1;
    state.particles.position_y_comoving[i] = static_cast<double>(i) * 0.2;
    state.particles.position_z_comoving[i] = static_cast<double>(i) * 0.3;
    state.particles.velocity_x_peculiar[i] = static_cast<double>(i) * 1.0;
    state.particles.velocity_y_peculiar[i] = static_cast<double>(i) * 2.0;
    state.particles.velocity_z_peculiar[i] = static_cast<double>(i) * 3.0;
    state.particles.mass_code[i] = 100.0 + static_cast<double>(i);
    state.particle_sidecar.particle_id[i] = 1000 + i;
    state.particle_sidecar.owning_rank[i] = 0;
  }

  state.particle_sidecar.species_tag[0] = static_cast<std::uint32_t>(cosmosim::core::ParticleSpecies::kDarkMatter);
  state.particle_sidecar.species_tag[1] = static_cast<std::uint32_t>(cosmosim::core::ParticleSpecies::kDarkMatter);
  state.particle_sidecar.species_tag[2] = static_cast<std::uint32_t>(cosmosim::core::ParticleSpecies::kGas);
  state.particle_sidecar.species_tag[3] = static_cast<std::uint32_t>(cosmosim::core::ParticleSpecies::kGas);
  state.particle_sidecar.species_tag[4] = static_cast<std::uint32_t>(cosmosim::core::ParticleSpecies::kStar);
  state.particle_sidecar.species_tag[5] = static_cast<std::uint32_t>(cosmosim::core::ParticleSpecies::kStar);

  state.metadata.scale_factor = 0.5;
  state.metadata.run_name = "snapshot_roundtrip";
  state.rebuildSpeciesIndex();
}

void testRoundtripMixedSpeciesSnapshot() {
  cosmosim::core::SimulationConfig config;
  config.output.run_name = "snapshot_roundtrip";

  cosmosim::core::SimulationState state;
  fillMixedSpeciesState(state);

  const std::filesystem::path snapshot_path =
      std::filesystem::temp_directory_path() / "cosmosim_snapshot_roundtrip.hdf5";

#if COSMOSIM_ENABLE_HDF5
  cosmosim::io::SnapshotWritePayload payload;
  payload.state = &state;
  payload.config = &config;
  payload.normalized_config_text = "schema_version=1\nmode=zoom_in\n";
  payload.provenance = cosmosim::core::makeProvenanceRecord("abc123", "deadbeef", 0);

  cosmosim::io::SnapshotIoPolicy policy;
  policy.enable_compression = true;
  policy.compression_level = 1;
  policy.chunk_particle_count = 2;
  cosmosim::io::writeGadgetArepoSnapshotHdf5(snapshot_path, payload, policy);

  const cosmosim::io::SnapshotReadResult roundtrip =
      cosmosim::io::readGadgetArepoSnapshotHdf5(snapshot_path, config);

  assert(roundtrip.state.particles.size() == state.particles.size());
  assert(roundtrip.state.validateUniqueParticleIds());
  assert(roundtrip.state.metadata.scale_factor == state.metadata.scale_factor);
  assert(roundtrip.normalized_config_text == payload.normalized_config_text);

  double checksum_in = 0.0;
  double checksum_out = 0.0;
  for (std::size_t i = 0; i < state.particles.size(); ++i) {
    checksum_in += state.particles.position_x_comoving[i] + state.particles.mass_code[i] * 0.001;
    checksum_out += roundtrip.state.particles.position_x_comoving[i] +
                    roundtrip.state.particles.mass_code[i] * 0.001;
  }
  assert(checksum_in == checksum_out);

  std::filesystem::remove(snapshot_path);
#else
  bool threw = false;
  try {
    cosmosim::io::SnapshotWritePayload payload;
    payload.state = &state;
    payload.config = &config;
    cosmosim::io::writeGadgetArepoSnapshotHdf5(snapshot_path, payload);
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);
#endif
}

}  // namespace

int main() {
  testRoundtripMixedSpeciesSnapshot();
  return 0;
}
