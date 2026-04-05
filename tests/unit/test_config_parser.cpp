#include <cassert>
#include <string>

#include "cosmosim/core/config.hpp"

namespace {

void testCommentsWhitespaceSectionsAndUnits() {
  const std::string config_text = R"(
# top-level comment
[units]
length_unit = mpc
coordinate_frame = comoving

[mode]
mode = zoom_in
ic_file = zoom_ics.hdf5
zoom_high_res_region = true
zoom_region_file = region_zoom.hdf5

[cosmology]
box_size = 50000 kpc
omega_matter = 0.31
omega_lambda = 0.69

[numerics]
time_begin_code = 0.0
time_end_code = 1.0
gravity_softening = 2.5 kpc

[output]
output_stem = snapshot
restart_stem = restart
)";

  const auto frozen = cosmosim::core::loadFrozenConfigFromString(config_text, "unit_case");
  assert(frozen.config.cosmology.box_size_mpc_comoving == 50.0);
  assert(frozen.config.numerics.gravity_softening_kpc_comoving == 2.5);
  assert(frozen.config.mode.zoom_high_res_region);
  assert(frozen.config.mode.zoom_region_file == "region_zoom.hdf5");
}

void testDuplicateKeyFails() {
  const std::string config_text = "[output]\noutput_stem = snapshot\noutput_stem = other\n";
  bool threw = false;
  try {
    (void)cosmosim::core::loadFrozenConfigFromString(config_text, "dup");
  } catch (const cosmosim::core::ConfigError&) {
    threw = true;
  }
  assert(threw);
}

void testMissingValueFails() {
  const std::string config_text = "mode.mode = zoom_in\noutput.output_stem =\n";
  bool threw = false;
  try {
    (void)cosmosim::core::loadFrozenConfigFromString(config_text, "missing_value");
  } catch (const cosmosim::core::ConfigError&) {
    threw = true;
  }
  assert(threw);
}

void testUnknownKeysFailUnlessCompatibilityEnabled() {
  const std::string config_text = "mode.mode = zoom_in\nfoo.bar = 1\n";
  bool threw = false;
  try {
    (void)cosmosim::core::loadFrozenConfigFromString(config_text, "unknown");
  } catch (const cosmosim::core::ConfigError&) {
    threw = true;
  }
  assert(threw);

  const std::string compat_text =
      "mode.mode = zoom_in\ncompatibility.allow_unknown_keys = true\nfoo.bar = 1\n";
  const auto frozen = cosmosim::core::loadFrozenConfigFromString(compat_text, "compat_unknown");
  assert(frozen.config.compatibility.allow_unknown_keys);
}

void testInvalidEnumFails() {
  const std::string config_text = "mode.mode = giant_universe\n";
  bool threw = false;
  try {
    (void)cosmosim::core::loadFrozenConfigFromString(config_text, "bad_enum");
  } catch (const cosmosim::core::ConfigError&) {
    threw = true;
  }
  assert(threw);
}

void testDefaultsCanonicalizationAndDeterminism() {
  const std::string first =
      "[mode]\nmode = cosmo_cube\n[output]\nrun_name = a\noutput_stem = snapshot\nrestart_stem = "
      "restart\n";
  const std::string second =
      "[output]\nrestart_stem = restart\noutput_stem = snapshot\nrun_name = a\n[mode]\nmode = "
      "cosmo_cube\n";

  const auto frozen_first = cosmosim::core::loadFrozenConfigFromString(first, "first");
  const auto frozen_second = cosmosim::core::loadFrozenConfigFromString(second, "second");

  assert(frozen_first.config.physics.enable_cooling);
  assert(frozen_first.config.parallel.deterministic_reduction);
  assert(frozen_first.normalized_text == frozen_second.normalized_text);
  assert(frozen_first.provenance.config_hash_hex == frozen_second.provenance.config_hash_hex);
}

}  // namespace

int main() {
  testCommentsWhitespaceSectionsAndUnits();
  testDuplicateKeyFails();
  testMissingValueFails();
  testUnknownKeysFailUnlessCompatibilityEnabled();
  testInvalidEnumFails();
  testDefaultsCanonicalizationAndDeterminism();
  return 0;
}
