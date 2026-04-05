#include "cosmosim/core/version.hpp"

#include <sstream>

namespace cosmosim::core {

Version version() {
  return Version{COSMOSIM_VERSION_MAJOR, COSMOSIM_VERSION_MINOR, COSMOSIM_VERSION_PATCH};
}

std::string versionString() {
  const Version v = version();
  std::ostringstream stream;
  stream << v.major << "." << v.minor << "." << v.patch;
  return stream.str();
}

std::string buildProvenance() {
  std::ostringstream stream;
  stream << "project=" << projectName() << ";version=" << versionString();
  return stream.str();
}

std::string_view projectName() {
  return "cosmosim";
}

}  // namespace cosmosim::core
