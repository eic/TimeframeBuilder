#pragma once

#ifdef HAVE_ARROW

#include "EDM4hepDataHandler.h"

/**
 * @class ArrowInputDataHandler
 * @brief EDM4hepDataHandler variant that reads from Arrow IPC streams via ArrowDataSource.
 *
 * Inherits all output/merging logic from EDM4hepDataHandler (ROOT TTree output)
 * but creates ArrowDataSource instances instead of EDM4hepDataSource, so
 * that input reading comes from Arrow IPC streams (files, FIFOs, pipes, stdin).
 *
 * The only override is initializeDataSources(); everything else — collection
 * discovery, output writing, finalization — is identical to the ROOT backend.
 */
class ArrowInputDataHandler : public EDM4hepDataHandler {
public:
    ArrowInputDataHandler() = default;
    ~ArrowInputDataHandler() override = default;

    std::vector<std::unique_ptr<DataSource>> initializeDataSources(
        const std::string& filename,
        const std::vector<SourceConfig>& source_configs) override;

    std::string getFormatName() const override { return "EDM4hep (Arrow IPC input)"; }
};

#endif  // HAVE_ARROW
