#pragma once

#include "EDM4hepDataSource.h"
#include <podio/ROOTReader.h>
#include <podio/Frame.h>
#include <optional>
#include <unordered_map>

/**
 * @class PodioEDM4hepDataSource
 * @brief EDM4hep data source using the podio::ROOTReader interface.
 *
 * Replaces direct TChain/SetBranchAddress access with the official podio Frame
 * reader. Reads typed edm4hep collections from podio::Frame objects and converts
 * them to the same raw *Data structs expected by EDM4hepDataHandler so that both
 * backends can be compared with identical output.
 */
class PodioEDM4hepDataSource : public EDM4hepDataSource {
public:
    PodioEDM4hepDataSource(const SourceConfig& config, size_t source_index);
    ~PodioEDM4hepDataSource() override = default;

    // DataSource interface
    void initialize(const std::vector<std::string>& tracker_collections,
                    const std::vector<std::string>& calo_collections,
                    const std::vector<std::string>& gp_collections) override;

    bool hasMoreEntries() const override;
    bool loadNextEvent() override;
    void loadEvent(size_t event_index) override;

    bool isInitialized() const override { return initialized_; }
    std::string getFormatName() const override { return "EDM4hep (podio)"; }
    void printStatus() const override;

    // Data processing — override virtual methods from EDM4hepDataSource
    std::vector<edm4hep::MCParticleData>& processMCParticles(size_t particle_parents_offset,
                                                             size_t particle_daughters_offset,
                                                             int totalEventsConsumed) override;

    std::vector<podio::ObjectID>& processObjectID(const std::string& collection_name,
                                                  size_t index_offset,
                                                  int totalEventsConsumed) override;

    std::vector<edm4hep::SimTrackerHitData>& processTrackerHits(const std::string& collection_name,
                                                                size_t particle_index_offset,
                                                                int totalEventsConsumed) override;

    std::vector<edm4hep::SimCalorimeterHitData>& processCaloHits(const std::string& collection_name,
                                                                  size_t particle_index_offset,
                                                                  int totalEventsConsumed) override;

    std::vector<edm4hep::CaloHitContributionData>& processCaloContributions(const std::string& collection_name,
                                                                             size_t particle_index_offset,
                                                                             int totalEventsConsumed) override;

    std::vector<std::string>& processGPBranch(const std::string& branch_name) override;
    std::vector<std::vector<int>>& processGPIntValues() override;
    std::vector<std::vector<float>>& processGPFloatValues() override;
    std::vector<std::vector<double>>& processGPDoubleValues() override;
    std::vector<std::vector<std::string>>& processGPStringValues() override;

    std::vector<edm4hep::EventHeaderData>& processEventHeaders(const std::string& collection_name) override;

    /// Expose available collection names so the handler can discover them
    std::vector<std::string> getAvailableCollections() const;

protected:
    podio::ROOTReader reader_;
    bool initialized_{false};
    // Tracks the next index that readNextEntry() would return, so we can
    // use the cheaper sequential API instead of readEntry(n) when possible.
    size_t next_sequential_index_{0};

    // Collection name lists (set by initialize())
    const std::vector<std::string>* tracker_collection_names_{nullptr};
    const std::vector<std::string>* calo_collection_names_{nullptr};
    const std::vector<std::string>* gp_collection_names_{nullptr};

    // The current frame, held until all process* calls have consumed it lazily.
    std::optional<podio::Frame> current_frame_;

    // Helpers
    void storeFrame(podio::Frame&& frame);

private:
    // Extraction-done flags (set to true after the corresponding extract* runs).
    bool extracted_mcparticles_{false};
    bool extracted_tracker_hits_{false};
    bool extracted_calo_hits_{false};
    bool extracted_event_headers_{false};
    bool extracted_gp_{false};

    // Cached data extracted from the current frame.
    // vec.clear() preserves allocated capacity; emplace_back reuses it event-to-event.
    std::vector<edm4hep::MCParticleData> cached_mcparticles_;
    std::unordered_map<std::string, std::vector<podio::ObjectID>> cached_objectids_;
    std::unordered_map<std::string, std::vector<edm4hep::SimTrackerHitData>> cached_tracker_hits_;
    std::unordered_map<std::string, std::vector<edm4hep::SimCalorimeterHitData>> cached_calo_hits_;
    std::unordered_map<std::string, std::vector<edm4hep::CaloHitContributionData>> cached_calo_contribs_;
    std::unordered_map<std::string, std::vector<edm4hep::EventHeaderData>> cached_event_headers_;

    // GP caches
    std::unordered_map<std::string, std::vector<std::string>> cached_gp_key_branches_;
    std::vector<std::vector<int>> cached_gp_int_values_;
    std::vector<std::vector<float>> cached_gp_float_values_;
    std::vector<std::vector<double>> cached_gp_double_values_;
    std::vector<std::vector<std::string>> cached_gp_string_values_;

    void ensureMCParticlesExtracted();
    void ensureTrackerHitsExtracted();
    void ensureCaloHitsExtracted();
    void ensureEventHeadersExtracted();
    void ensureGPExtracted();
    void extractMCParticles(const podio::Frame& frame);
    void extractTrackerHits(const podio::Frame& frame);
    void extractCaloHits(const podio::Frame& frame);
    void extractEventHeaders(const podio::Frame& frame);
    void extractGP(const podio::Frame& frame);
    void clearCaches();

    VertexPosition getBeamVertexPosition() const override;

    static std::string contribCollectionName(const std::string& calo_name) {
        return calo_name + "Contributions";
    }
};
