#include <cassert>
#include <stdexcept>

#include "cosmosim/core/build_config.hpp"
#include "cosmosim/io/ic_reader.hpp"

namespace {

void testGeneratedIsolatedIcSpeciesAndOwnership() {
  cosmosim::core::SimulationConfig config;
  config.output.run_name = "unit_ic_reader";

  const cosmosim::io::IcReadResult result =
      cosmosim::io::buildGeneratedIsolatedIc(config, 5, 3, 1000);

  assert(result.state.particles.size() == 8);
  assert(result.state.species.count_by_species[static_cast<std::size_t>(cosmosim::core::ParticleSpecies::kDarkMatter)] ==
         5);
  assert(result.state.species.count_by_species[static_cast<std::size_t>(cosmosim::core::ParticleSpecies::kGas)] == 3);
  assert(result.state.validateOwnershipInvariants());
}

void testGeneratedConverterDefaultAudit() {
  cosmosim::core::SimulationConfig config;
  const cosmosim::io::IcReadResult result = cosmosim::io::convertGeneratedIsolatedIcToState(config, 4);

  assert(result.state.particles.size() == 20);
  assert(!result.report.defaulted_fields.empty());
}

void testHdf5GateBehavior() {
#if COSMOSIM_ENABLE_HDF5
  cosmosim::core::SimulationConfig config;
  bool threw = false;
  try {
    const auto result = cosmosim::io::readGadgetArepoHdf5Ic("/definitely/missing/file.hdf5", config);
    (void)result;
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);
#else
  cosmosim::core::SimulationConfig config;
  bool threw = false;
  try {
    const auto result = cosmosim::io::readGadgetArepoHdf5Ic("ics.hdf5", config);
    (void)result;
  } catch (const std::runtime_error&) {
    threw = true;
  }
  assert(threw);
#endif
}

}  // namespace

int main() {
  testGeneratedIsolatedIcSpeciesAndOwnership();
  testGeneratedConverterDefaultAudit();
  testHdf5GateBehavior();
  return 0;
}
