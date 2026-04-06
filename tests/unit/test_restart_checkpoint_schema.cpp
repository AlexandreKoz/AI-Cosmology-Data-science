#include <cassert>

#include "cosmosim/io/restart_checkpoint.hpp"

int main() {
  const auto& schema = cosmosim::io::restartSchema();
  assert(schema.name == "cosmosim_restart_v1");
  assert(schema.version == 1);
  assert(cosmosim::io::isRestartSchemaCompatible(1));
  assert(!cosmosim::io::isRestartSchemaCompatible(2));
  return 0;
}
