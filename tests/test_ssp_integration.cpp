#include "cosmosim/stellar_evolution_bookkeeping.h"

#include <cmath>
#include <iostream>
#include <string>

using cosmosim::ChannelWeights;
using cosmosim::EvolutionTableProvenance;
using cosmosim::StarPopulationState;
using cosmosim::StellarEvolutionBookkeeper;

#ifndef COSMOSIM_SOURCE_DIR
#define COSMOSIM_SOURCE_DIR "."
#endif


namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}

}  // namespace

int main() {
    const std::string table_path = std::string(COSMOSIM_SOURCE_DIR) + "/resources/stellar_evolution/default_table.csv";
    EvolutionTableProvenance provenance{"v1_default", table_path,
                                         "stellar_evo_schema_v1"};
    auto table = StellarEvolutionBookkeeper::loadEvolutionTableCsv(provenance.source_path, provenance);

    ChannelWeights channel_weights{0.25, 0.60, 0.15};
    StellarEvolutionBookkeeper bookkeeper(table, channel_weights);

    // Small SSP integration scenario across two timesteps.
    StarPopulationState stars;
    stars.current_mass_msun = {800.0, 1200.0, 600.0};
    stars.birth_mass_msun = {800.0, 1200.0, 600.0};
    stars.age_yr = {1.0e7, 1.0e7, 1.0e7};
    stars.metallicity_mass_fraction = {0.02, 0.01, 0.015};

    const auto out_1 = bookkeeper.advancePopulation(stars, 9.0e7, 1.0e-9);
    const auto out_2 = bookkeeper.advancePopulation(stars, 9.0e8, 1.0e-9);

    const double initial_total_mass = 2600.0;
    const double final_total_mass = stars.current_mass_msun[0] + stars.current_mass_msun[1] +
                                    stars.current_mass_msun[2];

    double total_returned = 0.0;
    for (const auto& delta : out_1.per_star) {
        total_returned += delta.returned_mass_msun;
    }
    for (const auto& delta : out_2.per_star) {
        total_returned += delta.returned_mass_msun;
    }

    require(std::abs((initial_total_mass - final_total_mass) - total_returned) < 1.0e-6,
            "SSP total mass loss matches integrated returned mass");

    // Regression check against table cumulative return at 1e9 yr = 0.30 from age 1e7.
    const double expected_return_fraction = 0.30 - 0.08;
    require(std::abs(total_returned - initial_total_mass * expected_return_fraction) < 1.0e-6,
            "SSP regression return fraction at 1e9 yr");

    std::cout << "PASS: test_ssp_integration\n";
    return 0;
}
