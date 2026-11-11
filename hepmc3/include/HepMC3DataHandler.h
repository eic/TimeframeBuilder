#pragma once

#include "DataHandler.h"
#include "HepMC3DataSource.h"
#include <HepMC3/WriterRootTree.h>
#include <HepMC3/GenEvent.h>
#include <HepMC3/GenVertex.h>
#include <HepMC3/GenParticle.h>
#include <memory>
#include <vector>
#include <string>

/**
 * @class HepMC3DataHandler
 * @brief Concrete implementation of DataHandler for HepMC3 format
 * 
 * Handles both input (creating HepMC3DataSource instances) and output
 * (writing merged timeframe data) in HepMC3 format using ROOT tree format.
 * Merges multiple HepMC3 events into single timeframe events with time offsets applied.
 */
class HepMC3DataHandler : public DataHandler {
public:
    HepMC3DataHandler() = default;
    ~HepMC3DataHandler() override = default;

    std::vector<std::unique_ptr<DataSource>> initializeDataSources(
        const std::string& filename,
        const std::vector<SourceConfig>& source_configs) override;

    void initializeOutput(
        const MergerConfig& config,
        const TransformationConfig& transformations,
        const std::vector<std::unique_ptr<DataSource>>& data_sources) override;

    std::shared_ptr<HepMC3::GenRunInfo> configureMetadata(const MergerConfig& config);
    
    void prepareTimeframe() override;
    
    void writeTimeframe() override;
    
    void finalize() override;
    
    std::string getFormatName() const override { return "HepMC3"; }

private:
    std::shared_ptr<HepMC3::WriterRootTree> writer_;
    std::unique_ptr<HepMC3::GenEvent> current_timeframe_;
    
    // Store validated HepMC3 data sources (non-owning pointers)
    std::vector<HepMC3DataSource*> hepmc3_sources_;
    
    // Format-specific event processing
    void processEvent(DataSource& source) override;

    // Helper methods
    long insertHepMC3Event(const HepMC3::GenEvent& inevt,
                          std::unique_ptr<HepMC3::GenEvent>& hepframe,
                          bool keepWeights);
};
