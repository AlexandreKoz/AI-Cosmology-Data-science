#include "cosmosim/utils/provenance.hpp"

#include <sstream>

namespace cosmosim::utils {

std::string build_provenance_tag() {
  std::ostringstream out;
#ifdef COSMOSIM_PROVENANCE_ENABLED
  out << "provenance=on";
#else
  out << "provenance=off";
#endif
#ifdef COSMOSIM_OPENMP_ENABLED
  out << ";openmp=on";
#else
  out << ";openmp=off";
#endif
  return out.str();
}

} // namespace cosmosim::utils
