#pragma once

#include "DataHandler.h"
#include "PodioBufferMerger.h"
#include "PodioFrameBuilder.h"

#include <podio/CollectionIDTable.h>
#include <podio/GenericParameters.h>
#include <podio/ROOTWriter.h>

#include <cstdint>
#include <memory>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>

class PodioDataHandler final : public DataHandler {
public:
    ~PodioDataHandler() override;

    std::vector<std::unique_ptr<DataSource>> initializeDataSources(
        const std::string& filename,
        const std::vector<SourceConfig>& source_configs) override;

    void initializeOutput(
        const MergerConfig& config,
        const TransformationConfig& transformations,
        const std::vector<std::unique_ptr<DataSource>>& data_sources) override;

    void prepareTimeframe() override;
    void writeTimeframe() override;
    void finalize() override;
    std::string getFormatName() const override { return "podio"; }

protected:
    void processEvent(DataSource& source) override;

private:
    struct CollectionSchema {
        std::string type;
        podio::SchemaVersionT version{0};
    };

    void createEmptyCollections();

    std::unique_ptr<podio::ROOTWriter> writer_;
    PodioCollectionBufferMap collection_buffers_;
    podio::CollectionIDTable collection_ids_;
    std::map<std::string, CollectionSchema> collection_schema_;
    std::unique_ptr<podio::GenericParameters> timeframe_parameters_;
    std::optional<std::string> event_header_type_;
    uint64_t total_events_consumed_{0};
};
