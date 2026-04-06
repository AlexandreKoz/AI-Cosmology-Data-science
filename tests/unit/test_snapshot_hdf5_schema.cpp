#include <cassert>

#include "cosmosim/io/snapshot_hdf5.hpp"

namespace {

void testCanonicalSchemaNames() {
  const auto& schema = cosmosim::io::gadgetArepoSchemaMap();
  assert(schema.header_group == "/Header");
  assert(schema.part_type_group[0] == "/PartType0");
  assert(schema.coordinates.canonical_name == "Coordinates");
  assert(schema.velocities.canonical_name == "Velocities");
  assert(schema.particle_ids.canonical_name == "ParticleIDs");

  bool has_particle_id_alias = false;
  for (std::string_view alias : schema.particle_ids.read_aliases) {
    if (alias == "ParticleID") {
      has_particle_id_alias = true;
    }
  }
  assert(has_particle_id_alias);
}

}  // namespace

int main() {
  testCanonicalSchemaNames();
  return 0;
}
