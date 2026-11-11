#pragma once

#include "DataSource.h"

#include <podio/ROOTFrameData.h>
#include <podio/ROOTReader.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class PodioDataSource final : public DataSource {
public:
    PodioDataSource(const SourceConfig& config, size_t source_index);
    ~PodioDataSource() override;

    void initialize(const std::vector<std::string>& tracker_collections,
                    const std::vector<std::string>& calo_collections,
                    const std::vector<std::string>& gp_collections) override;

    bool hasMoreEntries() const override;
    bool loadNextEvent() override;
    void loadEvent(size_t event_index) override;

    std::unique_ptr<podio::ROOTFrameData> takeCurrentFrameData();
    std::unordered_map<std::string, podio::CollectionReadBuffers>
    takeCurrentCollectionBuffers();
    const std::unordered_map<std::string, podio::CollectionReadBuffers>&
    getCurrentCollectionBuffers() const;

    void printStatus() const override;
    bool isInitialized() const override;
    std::string getFormatName() const override { return "podio"; }

protected:
    VertexPosition getBeamVertexPosition() const override;

private:
    std::unique_ptr<podio::ROOTReader> reader_;
    std::unique_ptr<podio::ROOTFrameData> current_frame_data_;
    std::unordered_map<std::string, podio::CollectionReadBuffers>
        current_collection_buffers_;
    size_t loaded_entry_index_{0};
};
