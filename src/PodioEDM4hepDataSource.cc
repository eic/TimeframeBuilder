#include "PodioEDM4hepDataSource.h"
#include <edm4hep/MCParticleCollection.h>
#include <edm4hep/SimTrackerHitCollection.h>
#include <edm4hep/SimCalorimeterHitCollection.h>
#include <edm4hep/CaloHitContributionCollection.h>
#include <edm4hep/EventHeaderCollection.h>
#include <podio/Frame.h>
#include <iostream>
#include <limits>

PodioEDM4hepDataSource::PodioEDM4hepDataSource(const SourceConfig& config, size_t source_index)
    : EDM4hepDataSource(config, source_index) {
    total_entries_ = 0;
    current_entry_index_ = 0;
    entries_needed_ = 1;
}

void PodioEDM4hepDataSource::initialize(const std::vector<std::string>& tracker_collections,
                                        const std::vector<std::string>& calo_collections,
                                        const std::vector<std::string>& gp_collections) {
    // Always update collection name pointers — may be called again after discovery
    if (!tracker_collections.empty()) tracker_collection_names_ = &tracker_collections;
    if (!calo_collections.empty())    calo_collection_names_    = &calo_collections;
    if (!gp_collections.empty())      gp_collection_names_      = &gp_collections;

    // Guard the file-open against double-initialization
    if (initialized_) return;

    if (!config_->input_files.empty()) {
        try {
            reader_.openFiles(config_->input_files);

            for (const auto& f : config_->input_files) {
                std::cout << "Added file to podio source " << source_index_ << ": " << f << std::endl;
            }

            total_entries_ = reader_.getEntries(config_->tree_name);
            if (total_entries_ == 0) {
                throw std::runtime_error("No entries found in podio source " + std::to_string(source_index_));
            }
            std::cout << "Podio source " << source_index_ << " has " << total_entries_ << " entries" << std::endl;

            if (config_->skip > 0) {
                if (static_cast<size_t>(config_->skip) >= total_entries_ && !config_->repeat_on_eof) {
                    throw std::runtime_error("Skip value exceeds total entries for source " +
                                             std::to_string(source_index_));
                }
                current_entry_index_ = static_cast<size_t>(config_->skip) % total_entries_;
                std::cout << "Skipping first " << config_->skip << " events for source " << source_index_
                          << ", starting at entry " << current_entry_index_ << std::endl;
            }

            initialized_ = true;
            // next_sequential_index_ starts at SIZE_MAX (invalid) so the first
            // loadEvent() call always uses readEntry() to seek, which internally
            // positions the reader for readNextEntry() on subsequent calls.
            next_sequential_index_ = std::numeric_limits<size_t>::max();
            std::cout << "Successfully initialized podio EDM4hep source " << source_index_
                      << " (" << config_->name << ")" << std::endl;

        } catch (const std::exception& e) {
            throw std::runtime_error("ERROR: Could not open input files for podio source " +
                                     std::to_string(source_index_) + ": " + e.what());
        }
    }
}

bool PodioEDM4hepDataSource::hasMoreEntries() const {
    if (config_->repeat_on_eof && total_entries_ > 0) {
        return true;
    }
    return (current_entry_index_ + entries_needed_) <= total_entries_;
}

bool PodioEDM4hepDataSource::loadNextEvent() {
    if (current_entry_index_ >= total_entries_) {
        if (config_->repeat_on_eof) {
            current_entry_index_ = 0;
        }
        return false;
    }
    auto frame_data = reader_.readNextEntry(config_->tree_name);
    if (!frame_data) {
        return false;
    }
    ++next_sequential_index_;
    storeFrame(podio::Frame(std::move(frame_data)));
    return true;
}

void PodioEDM4hepDataSource::loadEvent(size_t event_index) {
    size_t actual_index = (config_->repeat_on_eof && total_entries_ > 0)
                              ? event_index % total_entries_
                              : event_index;

    std::unique_ptr<podio::ROOTFrameData> frame_data;
    if (actual_index == next_sequential_index_) {
        // Sequential path: avoids category-map lookup in readEntry()
        frame_data = reader_.readNextEntry(config_->tree_name);
        ++next_sequential_index_;
    } else {
        // Non-sequential seek (repeat_on_eof wrap-around or skip)
        frame_data = reader_.readEntry(config_->tree_name, actual_index);
        // Resync the sequential counter so future sequential reads are fast
        next_sequential_index_ = actual_index + 1;
    }

    if (!frame_data) {
        throw std::runtime_error("PodioEDM4hepDataSource: failed to read entry " +
                                 std::to_string(actual_index));
    }
    storeFrame(podio::Frame(std::move(frame_data)));
}

// ---------------------------------------------------------------------------
// Frame storage — lazy-extraction entry point
// ---------------------------------------------------------------------------
void PodioEDM4hepDataSource::storeFrame(podio::Frame&& frame) {
    clearCaches();
    current_frame_ = std::move(frame);
}

void PodioEDM4hepDataSource::clearCaches() {
    // vec.clear() preserves allocated capacity so subsequent emplace_back calls
    // reuse the same memory without reallocating (key perf benefit across events).
    cached_mcparticles_.clear();
    for (auto& [k, v] : cached_objectids_)    v.clear();
    for (auto& [k, v] : cached_tracker_hits_) v.clear();
    for (auto& [k, v] : cached_calo_hits_)    v.clear();
    for (auto& [k, v] : cached_calo_contribs_) v.clear();
    for (auto& [k, v] : cached_event_headers_) v.clear();
    for (auto& [k, v] : cached_gp_key_branches_) v.clear();
    cached_gp_int_values_.clear();
    cached_gp_float_values_.clear();
    cached_gp_double_values_.clear();
    cached_gp_string_values_.clear();

    // Reset lazy-extraction flags.
    extracted_mcparticles_   = false;
    extracted_tracker_hits_  = false;
    extracted_calo_hits_     = false;
    extracted_event_headers_ = false;
    extracted_gp_            = false;
}

// ---------------------------------------------------------------------------
// Lazy-extraction guards
// ---------------------------------------------------------------------------
void PodioEDM4hepDataSource::ensureMCParticlesExtracted() {
    if (extracted_mcparticles_ || !current_frame_) return;
    extractMCParticles(*current_frame_);
    extracted_mcparticles_ = true;
}

void PodioEDM4hepDataSource::ensureTrackerHitsExtracted() {
    if (extracted_tracker_hits_ || !current_frame_) return;
    if (tracker_collection_names_) extractTrackerHits(*current_frame_);
    extracted_tracker_hits_ = true;
}

void PodioEDM4hepDataSource::ensureCaloHitsExtracted() {
    if (extracted_calo_hits_ || !current_frame_) return;
    if (calo_collection_names_) extractCaloHits(*current_frame_);
    extracted_calo_hits_ = true;
}

void PodioEDM4hepDataSource::ensureEventHeadersExtracted() {
    if (extracted_event_headers_ || !current_frame_) return;
    extractEventHeaders(*current_frame_);
    extracted_event_headers_ = true;
}

void PodioEDM4hepDataSource::ensureGPExtracted() {
    if (extracted_gp_ || !current_frame_) return;
    extractGP(*current_frame_);
    extracted_gp_ = true;
}

// ---------------------------------------------------------------------------
// MCParticles
// ---------------------------------------------------------------------------
void PodioEDM4hepDataSource::extractMCParticles(const podio::Frame& frame) {
    const auto* base = frame.get("MCParticles");
    if (!base) return;
    const auto& coll = frame.get<edm4hep::MCParticleCollection>("MCParticles");

    auto& parents_refs   = cached_objectids_["_MCParticles_parents"];
    auto& daughters_refs = cached_objectids_["_MCParticles_daughters"];

    cached_mcparticles_.reserve(coll.size());

    for (const auto& p : coll) {
        edm4hep::MCParticleData d;
        d.PDG                = p.getPDG();
        d.generatorStatus    = p.getGeneratorStatus();
        d.simulatorStatus    = p.getSimulatorStatus();
        d.charge             = p.getCharge();
        d.time               = p.getTime();
        d.mass               = p.getMass();
        d.vertex             = p.getVertex();
        d.endpoint           = p.getEndpoint();
        d.momentum           = p.getMomentum();
        d.momentumAtEndpoint = p.getMomentumAtEndpoint();
        d.helicity           = p.getHelicity();

        d.parents_begin = static_cast<unsigned int>(parents_refs.size());
        for (const auto& par : p.getParents()) {
            parents_refs.emplace_back(par.getObjectID());
        }
        d.parents_end = static_cast<unsigned int>(parents_refs.size());

        d.daughters_begin = static_cast<unsigned int>(daughters_refs.size());
        for (const auto& dau : p.getDaughters()) {
            daughters_refs.emplace_back(dau.getObjectID());
        }
        d.daughters_end = static_cast<unsigned int>(daughters_refs.size());

        cached_mcparticles_.emplace_back(d);
    }
}

// ---------------------------------------------------------------------------
// Tracker hits
// ---------------------------------------------------------------------------
void PodioEDM4hepDataSource::extractTrackerHits(const podio::Frame& frame) {
    for (const auto& name : *tracker_collection_names_) {
        const auto* base = frame.get(name);
        if (!base) continue;
        const auto& coll = frame.get<edm4hep::SimTrackerHitCollection>(name);

        auto& hits = cached_tracker_hits_[name];
        auto& refs = cached_objectids_["_" + name + "_particle"];

        hits.reserve(coll.size());
        refs.reserve(coll.size());

        for (const auto& h : coll) {
            edm4hep::SimTrackerHitData d;
            d.cellID     = h.getCellID();
            d.eDep       = h.getEDep();
            d.time       = h.getTime();
            d.pathLength = h.getPathLength();
            d.quality    = h.getQuality();
            d.position   = h.getPosition();
            d.momentum   = h.getMomentum();
            hits.emplace_back(d);
            refs.emplace_back(h.getParticle().getObjectID());
        }
    }
}

// ---------------------------------------------------------------------------
// Calorimeter hits + contributions
// ---------------------------------------------------------------------------
void PodioEDM4hepDataSource::extractCaloHits(const podio::Frame& frame) {
    for (const auto& name : *calo_collection_names_) {
        const auto* base = frame.get(name);
        if (!base) continue;
        const auto& coll = frame.get<edm4hep::SimCalorimeterHitCollection>(name);

        std::string contrib_name = contribCollectionName(name);
        const auto* contrib_base = frame.get(contrib_name);

        auto& hits              = cached_calo_hits_[name];
        auto& contrib_refs      = cached_objectids_["_" + name + "_contributions"];
        auto& contribs          = cached_calo_contribs_[name];
        auto& contrib_particle_refs = cached_objectids_["_" + contrib_name + "_particle"];

        hits.reserve(coll.size());

        for (const auto& h : coll) {
            edm4hep::SimCalorimeterHitData d;
            d.cellID   = h.getCellID();
            d.energy   = h.getEnergy();
            d.position = h.getPosition();

            d.contributions_begin = static_cast<unsigned int>(contrib_refs.size());
            for (const auto& c : h.getContributions()) {
                contrib_refs.emplace_back(c.getObjectID());

                if (contrib_base) {
                    edm4hep::CaloHitContributionData cd;
                    cd.PDG          = c.getPDG();
                    cd.energy       = c.getEnergy();
                    cd.time         = c.getTime();
                    cd.stepPosition = c.getStepPosition();
                    cd.stepLength   = c.getStepLength();
                    contribs.emplace_back(cd);
                    contrib_particle_refs.emplace_back(c.getParticle().getObjectID());
                }
            }
            d.contributions_end = static_cast<unsigned int>(contrib_refs.size());

            hits.emplace_back(d);
        }
    }
}

// ---------------------------------------------------------------------------
// Event headers
// ---------------------------------------------------------------------------
void PodioEDM4hepDataSource::extractEventHeaders(const podio::Frame& frame) {
    for (const auto& coll_name : {"EventHeader", "SubEventHeaders"}) {
        const auto* base = frame.get(coll_name);
        if (!base) continue;
        const auto& coll = frame.get<edm4hep::EventHeaderCollection>(coll_name);
        auto& headers = cached_event_headers_[coll_name];
        headers.reserve(coll.size());
        for (const auto& h : coll) {
            edm4hep::EventHeaderData d;
            d.eventNumber = h.getEventNumber();
            d.runNumber   = h.getRunNumber();
            d.timeStamp   = h.getTimeStamp();
            d.weight      = h.getWeight();
            headers.emplace_back(d);
        }
    }
}

// ---------------------------------------------------------------------------
// GenericParameters (GP)
// getKeysAndValues already returns owned vectors; move them directly into the
// caches to avoid any extra copy.
// ---------------------------------------------------------------------------
void PodioEDM4hepDataSource::extractGP(const podio::Frame& frame) {
    const auto& params = frame.getParameters();

    auto [int_keys, int_vals]       = params.getKeysAndValues<int>();
    auto [float_keys, float_vals]   = params.getKeysAndValues<float>();
    auto [double_keys, double_vals] = params.getKeysAndValues<double>();
    auto [str_keys, str_vals]       = params.getKeysAndValues<std::string>();

    cached_gp_key_branches_["GPIntKeys"]    = std::move(int_keys);
    cached_gp_key_branches_["GPFloatKeys"]  = std::move(float_keys);
    cached_gp_key_branches_["GPDoubleKeys"] = std::move(double_keys);
    cached_gp_key_branches_["GPStringKeys"] = std::move(str_keys);

    cached_gp_int_values_    = std::move(int_vals);
    cached_gp_float_values_  = std::move(float_vals);
    cached_gp_double_values_ = std::move(double_vals);
    cached_gp_string_values_ = std::move(str_vals);
}

// ---------------------------------------------------------------------------
// process* overrides — lazy extraction then modify in place
// ---------------------------------------------------------------------------

std::vector<edm4hep::MCParticleData>& PodioEDM4hepDataSource::processMCParticles(
    size_t particle_parents_offset, size_t particle_daughters_offset, int totalEventsConsumed) {

    ensureMCParticlesExtracted();

    if (totalEventsConsumed == 0 && config_->already_merged) {
        return cached_mcparticles_;
    }

    for (auto& p : cached_mcparticles_) {
        if (!config_->already_merged) {
            p.time += current_time_offset_;
            p.generatorStatus += config_->generator_status_offset;
        }
        p.parents_begin   += static_cast<unsigned int>(particle_parents_offset);
        p.parents_end     += static_cast<unsigned int>(particle_parents_offset);
        p.daughters_begin += static_cast<unsigned int>(particle_daughters_offset);
        p.daughters_end   += static_cast<unsigned int>(particle_daughters_offset);
    }
    return cached_mcparticles_;
}

std::vector<podio::ObjectID>& PodioEDM4hepDataSource::processObjectID(
    const std::string& collection_name, size_t index_offset, int totalEventsConsumed) {

    // ObjectIDs are populated as a side-effect of extractMCParticles/Tracker/CaloHits;
    // ensure the owning collection has been extracted first.
    if (collection_name == "_MCParticles_parents" || collection_name == "_MCParticles_daughters") {
        ensureMCParticlesExtracted();
    } else {
        // Determine origin: tracker or calo by checking collection name patterns.
        // A conservative approach: ensure all relevant collections are extracted.
        ensureTrackerHitsExtracted();
        ensureCaloHitsExtracted();
    }

    auto& refs = cached_objectids_[collection_name];

    if (totalEventsConsumed == 0 && config_->already_merged) {
        return refs;
    }
    for (auto& ref : refs) {
        ref.index += static_cast<int>(index_offset);
    }
    return refs;
}

std::vector<edm4hep::SimTrackerHitData>& PodioEDM4hepDataSource::processTrackerHits(
    const std::string& collection_name, size_t /*particle_index_offset*/, int totalEventsConsumed) {

    ensureTrackerHitsExtracted();

    auto& hits = cached_tracker_hits_[collection_name];
    if (totalEventsConsumed == 0 && config_->already_merged) {
        return hits;
    }
    if (!config_->already_merged) {
        for (auto& h : hits) {
            h.time += current_time_offset_;
        }
    }
    return hits;
}

std::vector<edm4hep::SimCalorimeterHitData>& PodioEDM4hepDataSource::processCaloHits(
    const std::string& collection_name, size_t contribution_index_offset, int totalEventsConsumed) {

    ensureCaloHitsExtracted();

    auto& hits = cached_calo_hits_[collection_name];
    if (totalEventsConsumed == 0 && config_->already_merged) {
        return hits;
    }
    for (auto& h : hits) {
        h.contributions_begin += static_cast<unsigned int>(contribution_index_offset);
        h.contributions_end   += static_cast<unsigned int>(contribution_index_offset);
    }
    return hits;
}

std::vector<edm4hep::CaloHitContributionData>& PodioEDM4hepDataSource::processCaloContributions(
    const std::string& collection_name, size_t /*particle_index_offset*/, int totalEventsConsumed) {

    ensureCaloHitsExtracted();

    auto& contribs = cached_calo_contribs_[collection_name];
    if (totalEventsConsumed == 0 && config_->already_merged) {
        return contribs;
    }
    if (!config_->already_merged) {
        for (auto& c : contribs) {
            c.time += current_time_offset_;
        }
    }
    return contribs;
}

std::vector<std::string>& PodioEDM4hepDataSource::processGPBranch(const std::string& branch_name) {
    ensureGPExtracted();
    return cached_gp_key_branches_[branch_name];
}

std::vector<std::vector<int>>& PodioEDM4hepDataSource::processGPIntValues() {
    ensureGPExtracted();
    return cached_gp_int_values_;
}

std::vector<std::vector<float>>& PodioEDM4hepDataSource::processGPFloatValues() {
    ensureGPExtracted();
    return cached_gp_float_values_;
}

std::vector<std::vector<double>>& PodioEDM4hepDataSource::processGPDoubleValues() {
    ensureGPExtracted();
    return cached_gp_double_values_;
}

std::vector<std::vector<std::string>>& PodioEDM4hepDataSource::processGPStringValues() {
    ensureGPExtracted();
    return cached_gp_string_values_;
}

std::vector<edm4hep::EventHeaderData>& PodioEDM4hepDataSource::processEventHeaders(
    const std::string& collection_name) {

    ensureEventHeadersExtracted();

    static std::vector<edm4hep::EventHeaderData> empty;
    auto it = cached_event_headers_.find(collection_name);
    if (it == cached_event_headers_.end()) {
        empty.clear();
        return empty;
    }
    return it->second;
}

// ---------------------------------------------------------------------------
// Collection discovery helper
// ---------------------------------------------------------------------------
std::vector<std::string> PodioEDM4hepDataSource::getAvailableCollections() const {
    if (!initialized_) return {};
    auto frame_data = const_cast<podio::ROOTReader&>(reader_).readEntry(config_->tree_name, 0);
    if (!frame_data) return {};
    podio::Frame frame(std::move(frame_data));
    return frame.getAvailableCollections();
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------
DataSource::VertexPosition PodioEDM4hepDataSource::getBeamVertexPosition() const {
    VertexPosition vertex{0.0f, 0.0f, 0.0f};
    for (const auto& p : cached_mcparticles_) {
        if (p.generatorStatus == 1) {
            vertex.x = static_cast<float>(p.vertex.x);
            vertex.y = static_cast<float>(p.vertex.y);
            vertex.z = static_cast<float>(p.vertex.z);
            break;
        }
    }
    return vertex;
}

void PodioEDM4hepDataSource::printStatus() const {
    std::cout << "=== PodioEDM4hepDataSource Status ===" << std::endl;
    std::cout << "Source: " << source_index_ << " (" << config_->name << ")" << std::endl;
    std::cout << "Total entries: " << total_entries_ << std::endl;
    std::cout << "Current entry: " << current_entry_index_ << std::endl;
    std::cout << "Initialized: " << (initialized_ ? "Yes" : "No") << std::endl;
    std::cout << "=====================================" << std::endl;
}
