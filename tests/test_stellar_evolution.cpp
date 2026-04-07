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

    ChannelWeights channel_weights{0.30, 0.50, 0.20};
    StellarEvolutionBookkeeper bookkeeper(table, channel_weights);

    // Lifetime interpolation check.
    const double rf_1e7 = bookkeeper.interpolateCumulativeReturnFraction(1.0e7);
    const double rf_1e8 = bookkeeper.interpolateCumulativeReturnFraction(1.0e8);
    const double rf_mid = bookkeeper.interpolateCumulativeReturnFraction(5.5e7);
    require(std::abs(rf_1e7 - 0.08) < 1.0e-12, "return fraction at 1e7 yr");
    require(std::abs(rf_1e8 - 0.18) < 1.0e-12, "return fraction at 1e8 yr");
    require(std::abs(rf_mid - 0.13) < 1.0e-12, "linear interpolation at midpoint");

    StarPopulationState stars;
    stars.current_mass_msun = {1000.0};
    stars.birth_mass_msun = {1000.0};
    stars.age_yr = {1.0e7};
    stars.metallicity_mass_fraction = {0.02};

    const auto output = bookkeeper.advancePopulation(stars, 9.0e7, 1.0e-9);
    require(output.per_star.size() == 1, "single-star output size");

    const auto& delta = output.per_star[0];
    // Conservation check for m_old = m_new + returned + remnant_change.
    const double closure = delta.old_mass_msun - delta.new_mass_msun - delta.returned_mass_msun -
                           delta.remnant_change_msun;
    require(std::abs(closure) < 1.0e-9, "mass conservation closure");

    // Return fraction delta: 0.18 - 0.08 = 0.10 -> 100 Msun returned.
    require(std::abs(delta.returned_mass_msun - 100.0) < 1.0e-9, "returned mass from table delta");

    // Yield-channel consistency: channel sums must equal total budgets.
    double channel_sum_returned = 0.0;
    for (const auto& channel_budget : delta.per_channel) {
        channel_sum_returned += channel_budget.returned_mass_msun;
    }
    require(std::abs(channel_sum_returned - delta.returned_mass_msun) < 1.0e-9,
            "channel returned-mass sum consistency");

    std::cout << "PASS: test_stellar_evolution\n";
    return 0;
}
