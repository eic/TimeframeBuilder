#pragma once

#include "DataHandler.h"
#include "EDM4hepDataHandler.h"
#include "PodioEDM4hepDataSource.h"
#include <podio/ROOTWriter.h>
#include <podio/Frame.h>
#include <memory>
#include <string>
#include <vector>

/**
 * @class PodioROOTDataHandler
 * @brief DataHandler that reads via PodioEDM4hepDataSource and writes via podio::ROOTWriter.
 *
 * Shares processEvent / mergeEvents logic with EDM4hepDataHandler (inherited indirectly via
 * EDM4hepMergedCollections), but the output path uses proper podio Frame serialization instead
 * of a raw ROOT TTree, making the output readable by podio::ROOTReader.
 *
 * A protected virtual writeFrame() allows ArrowDataHandler to redirect the frame to Arrow IPC.
 */
class PodioROOTDataHandler : public DataHandler {
public:
    PodioROOTDataHandler() = default;
    ~PodioROOTDataHandler() override = default;

    std::vector<std::unique_ptr<DataSource>> initializeDataSources(
        const std::string& filename,
        const std::vector<SourceConfig>& source_configs) override;

    void initializeOutput(const MergerConfig& config,
                          const std::vector<std::unique_ptr<DataSource>>& data_sources) override;

    void prepareTimeframe() override;

    void writeTimeframe() override;

    void finalize() override;

    std::string getFormatName() const override { return "EDM4hep (podio ROOTWriter)"; }

protected:
    /// Convert the accumulated EDM4hepMergedCollections into a podio::Frame and write it.
    /// Subclasses may override to redirect writing (e.g. Arrow IPC).
    virtual void writeFrame(podio::Frame& frame);

    /// Build a podio::Frame from the accumulated merged collections.
    podio::Frame buildFrame();

    // Output
    std::unique_ptr<podio::ROOTWriter> root_writer_;
    std::string output_filename_;

    // Accumulated merged collections (same structure as EDM4hepDataHandler)
    EDM4hepMergedCollections collections_;

    // Discovered collection name lists (populated in initializeOutput)
    std::vector<std::string> tracker_collection_names_;
    std::vector<std::string> calo_collection_names_;
    std::vector<std::string> gp_collection_names_;

private:
    // Non-owning pointers to PodioEDM4hepDataSource objects (owned by the sources vector)
    std::vector<PodioEDM4hepDataSource*> podio_sources_;

    void discoverCollections(const std::vector<std::unique_ptr<DataSource>>& sources);

    // Format-specific event processing
    void processEvent(DataSource& source) override;
};
