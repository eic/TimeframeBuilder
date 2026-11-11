#pragma once

#include "DataSource.h"
#include "MergerConfig.h"
#include <HepMC3/ReaderRootTree.h>
#include <HepMC3/GenEvent.h>
#include <HepMC3/GenVertex.h>
#include <HepMC3/GenParticle.h>
#include <memory>
#include <vector>
#include <string>
#include <random>

/**
 * @class HepMC3DataSource
 * @brief Concrete implementation for reading HepMC3 format event data
 * 
 * Handles all HepMC3-specific logic for reading events from ROOT tree files
 * (.hepmc3.tree.root), applying time offsets, and providing data for merging.
 */
class HepMC3DataSource : public DataSource {
public:
    HepMC3DataSource(const SourceConfig& config, size_t source_index);
    ~HepMC3DataSource();
    
    // Data access
    bool hasMoreEntries() const override;
    bool loadNextEvent() override;
    
    // Event loading
    void loadEvent(size_t event_index) override;

    // HepMC3-specific data access methods
    const HepMC3::GenEvent& getCurrentEvent() const { return current_event_; }
    
    // Status and diagnostics
    void printStatus() const override;
    bool isInitialized() const override { return reader_ != nullptr; }
    std::string getFormatName() const override { return "HepMC3"; }

private:
    // HepMC3 reader and state
    std::shared_ptr<HepMC3::ReaderRootTree> reader_;
    
    // Current event
    HepMC3::GenEvent current_event_;    
    size_t m_current_file; // Index of the current input file being processed
    size_t m_total_files;  // Total number of input files
    long long m_still_to_skip; // Number of events still to skip
    
    // Private helper methods
    void openNextFile();
    void cleanup();
    
    // Format-specific vertex extraction from HepMC3 events (overrides base class)
    VertexPosition getBeamVertexPosition() const override;
};
