#pragma once

#include "EDM4hepDataHandler.h"
#include "PodioEDM4hepDataSource.h"

/**
 * @class PodioEDM4hepDataHandler
 * @brief EDM4hepDataHandler variant that uses PodioEDM4hepDataSource for input.
 *
 * Inherits all output/merging logic from EDM4hepDataHandler (ROOT TTree output)
 * but creates PodioEDM4hepDataSource instances instead of EDM4hepDataSource, so
 * that input reading goes through podio::ROOTFrameReader rather than raw TChain.
 *
 * The only override is initializeDataSources(); everything else — collection
 * discovery, branch setup, output writing, finalization — is identical to the
 * ROOT backend and is therefore not duplicated.
 */
class PodioEDM4hepDataHandler : public EDM4hepDataHandler {
public:
    PodioEDM4hepDataHandler() = default;
    ~PodioEDM4hepDataHandler() override = default;

    std::vector<std::unique_ptr<DataSource>> initializeDataSources(
        const std::string& filename,
        const std::vector<SourceConfig>& source_configs) override;

    std::string getFormatName() const override { return "EDM4hep (podio)"; }
};
