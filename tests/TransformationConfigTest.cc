#include "CommandLineParser.h"

#include <iostream>
#include <string>

int main() {
    std::string executable = "transformation_config_test";
    std::string config_option = "--config";
    std::string run_config =
        std::string(TIMEFRAME_BUILDER_SOURCE_DIR) + "/configs/config.yml";
    std::string transform_option = "--transform-config";
    std::string transform_config =
        std::string(TIMEFRAME_BUILDER_SOURCE_DIR) +
        "/configs/timeframe_builder/podio_transformations.yml";

    char* arguments[] = {
        executable.data(),
        config_option.data(),
        run_config.data(),
        transform_option.data(),
        transform_config.data(),
    };

    const auto config = CommandLineParser::parse(5, arguments);
    if (config.transformations.event_transformations.size() != 1 ||
        config.transformations.record_transformations.size() != 2) {
        std::cerr << "Transformation profile did not populate the expected rules\n";
        return 1;
    }

    const auto& status_rule =
        config.transformations.record_transformations[1];
    if (status_rule.datatype != "edm4hep::MCParticle" ||
        status_rule.collection != "MCParticles" ||
        status_rule.field != "generatorStatus" ||
        status_rule.parameters.at("context") != "generator_status_offset") {
        std::cerr << "Podio transformation rule bindings were not parsed correctly\n";
        return 1;
    }

    return 0;
}
