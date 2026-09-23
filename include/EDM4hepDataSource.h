#pragma once

#include "DataSource.h"
#include "MergerConfig.h"
#include <edm4hep/MCParticleData.h>
#include <edm4hep/SimTrackerHitData.h>
#include <edm4hep/SimCalorimeterHitData.h>
#include <edm4hep/CaloHitContributionData.h>
#include <edm4hep/EventHeaderData.h>
#include <podio/ObjectID.h>
#include <TChain.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <random>

/**
 * @class EDM4hepDataSource
 * @brief Concrete implementation for reading EDM4hep format event data
 * 
 * Handles all EDM4hep-specific logic for reading events from ROOT files
 * with TChain, managing branches, applying time offsets, and merging data.
 */
class EDM4hepDataSource : public DataSource {
public:
    EDM4hepDataSource(const SourceConfig& config, size_t source_index);
    ~EDM4hepDataSource();
    
    // Initialization
    void initialize(const std::vector<std::string>& tracker_collections,
                   const std::vector<std::string>& calo_collections,
                   const std::vector<std::string>& gp_collections) override;
    
    // Data access
    bool hasMoreEntries() const override;
    bool loadNextEvent() override;
    
    // Event loading
    void loadEvent(size_t event_index) override;

    // Data processing methods for EDM4hep format
    virtual std::vector<edm4hep::MCParticleData>& processMCParticles(size_t particle_parents_offset,
                                                             size_t particle_daughters_offset,
                                                             int totalEventsConsumed);
    
    virtual std::vector<podio::ObjectID>& processObjectID(const std::string& collection_name, 
                                                  size_t index_offset, int totalEventsConsumed);
    
    virtual std::vector<edm4hep::SimTrackerHitData>& processTrackerHits(const std::string& collection_name,
                                                              size_t particle_index_offset,
                                                              int totalEventsConsumed);
    
    virtual std::vector<edm4hep::SimCalorimeterHitData>& processCaloHits(const std::string& collection_name,
                                                                size_t particle_index_offset,
                                                                int totalEventsConsumed);
    virtual std::vector<edm4hep::CaloHitContributionData>& processCaloContributions(const std::string& collection_name,
                                                                          size_t particle_index_offset,
                                                                          int totalEventsConsumed);

    virtual std::vector<std::string>& processGPBranch(const std::string& branch_name);
    virtual std::vector<std::vector<int>>& processGPIntValues();
    virtual std::vector<std::vector<float>>& processGPFloatValues();
    virtual std::vector<std::vector<double>>& processGPDoubleValues();
    virtual std::vector<std::vector<std::string>>& processGPStringValues();
    
    // Event header processing
    virtual std::vector<edm4hep::EventHeaderData>& processEventHeaders(const std::string& collection_name);
    
    // Status and diagnostics
    void printStatus() const override;
    bool isInitialized() const override { return chain_ != nullptr; }
    std::string getFormatName() const override { return "EDM4hep"; }

private:
    // ROOT chain and state
    std::unique_ptr<TChain> chain_;
    
    // Collection names (references to shared data)
    const std::vector<std::string>* tracker_collection_names_;
    const std::vector<std::string>* calo_collection_names_;
    const std::vector<std::string>* gp_collection_names_;
    
    // Branch pointers for reading data as vectors
    std::vector<edm4hep::MCParticleData>* mcparticle_branch_;
    std::unordered_map<std::string, std::vector<edm4hep::SimTrackerHitData>*> tracker_hit_branches_;
    std::unordered_map<std::string, std::vector<edm4hep::SimCalorimeterHitData>*> calo_hit_branches_;
    std::unordered_map<std::string, std::vector<edm4hep::CaloHitContributionData>*> calo_contrib_branches_;
    std::unordered_map<std::string, std::vector<edm4hep::EventHeaderData>*> event_header_branches_;
    
    // Branch pointers for reading ObjectID references
    std::unordered_map<std::string, std::vector<podio::ObjectID>*> objectid_branches_;
    
    // Branch pointers for reading GP (Global Parameter) branches
    std::unordered_map<std::string, std::vector<std::string>*> gp_key_branches_;
    std::vector<std::vector<int>>* gp_int_branch_;
    std::vector<std::vector<float>>* gp_float_branch_;
    std::vector<std::vector<double>>* gp_double_branch_;
    std::vector<std::vector<std::string>>* gp_string_branch_;

    // Current event processing state
    size_t current_particle_index_offset_;
    
    // Private helper methods
    void setupBranches();
    void setupMCParticleBranches();
    void setupTrackerBranches();
    void setupCalorimeterBranches();
    void setupEventHeaderBranches();
    void setupGPBranches();
    void cleanup();
    
    // Format-specific vertex extraction from EDM4hep MCParticles (overrides base class)
    VertexPosition getBeamVertexPosition() const override;
    
    // Helper methods for collection name mapping
    std::string getCorrespondingContributionCollection(const std::string& calo_collection_name) const;
    std::string getCorrespondingCaloCollection(const std::string& contrib_collection_name) const;
};
