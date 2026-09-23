#include "PodioROOTDataHandler.h"
#include "EDM4hepFrameBuilder.h"
#include <edm4hep/MCParticleCollection.h>
#include <edm4hep/SimTrackerHitCollection.h>
#include <edm4hep/SimCalorimeterHitCollection.h>
#include <edm4hep/CaloHitContributionCollection.h>
#include <edm4hep/EventHeaderCollection.h>
#include <iostream>
#include <stdexcept>

std::vector<std::unique_ptr<DataSource>> PodioROOTDataHandler::initializeDataSources(
    const std::string& filename,
    const std::vector<SourceConfig>& source_configs)
{
    std::cout << "Initializing PodioROOTDataHandler for: " << filename << std::endl;

    std::vector<std::unique_ptr<DataSource>> data_sources;
    data_sources.reserve(source_configs.size());

    for (size_t idx = 0; idx < source_configs.size(); ++idx) {
        const auto& cfg = source_configs[idx];
        if (cfg.input_files.empty()) {
            throw std::runtime_error("Source " + cfg.name + " has no input files");
        }
        data_sources.push_back(std::make_unique<PodioEDM4hepDataSource>(cfg, idx));
        std::cout << "Created PodioEDM4hepDataSource for: " << cfg.input_files[0] << std::endl;
    }

    podio_sources_.clear();
    podio_sources_.reserve(data_sources.size());
    for (auto& src : data_sources) {
        podio_sources_.push_back(dynamic_cast<PodioEDM4hepDataSource*>(src.get()));
    }

    output_filename_ = filename;
    return data_sources;
}

void PodioROOTDataHandler::initializeOutput(
    const MergerConfig& config,
    const std::vector<std::unique_ptr<DataSource>>& data_sources)
{
    output_filename_ = config.output_file;
    discoverCollections(data_sources);
    // ROOTWriter is created lazily on the first writeFrame call so that
    // ArrowDataHandler can override writeFrame without creating a stale file.
    std::cout << "PodioROOTDataHandler output initialized: " << output_filename_ << std::endl;
}

void PodioROOTDataHandler::prepareTimeframe()
{
    collections_.clear();
}

void PodioROOTDataHandler::writeTimeframe()
{
    // Add the timeframe-level EventHeader
    edm4hep::EventHeaderData header{};
    header.eventNumber = static_cast<uint64_t>(current_timeframe_number_);
    header.runNumber   = 0;
    header.timeStamp   = static_cast<uint64_t>(current_timeframe_number_);
    collections_.event_headers.push_back(header);

    auto frame = buildFrame();
    writeFrame(frame);
    std::cout << "=== Timeframe written (podio) ===" << std::endl;
}

void PodioROOTDataHandler::finalize()
{
    if (root_writer_) {
        root_writer_->finish();
        root_writer_.reset();
    }
    std::cout << "PodioROOTDataHandler output finalized" << std::endl;
}

// ---------------------------------------------------------------------------
// protected virtual — base implementation writes via podio::ROOTWriter
// ---------------------------------------------------------------------------
void PodioROOTDataHandler::writeFrame(podio::Frame& frame)
{
    if (!root_writer_) {
        root_writer_ = std::make_unique<podio::ROOTWriter>(output_filename_);
    }
    root_writer_->writeFrame(frame, "events");
}

// ---------------------------------------------------------------------------
// buildFrame — delegate to shared EDM4hepFrameBuilder utility
// ---------------------------------------------------------------------------
podio::Frame PodioROOTDataHandler::buildFrame()
{
    return buildEDM4hepFrame(collections_, tracker_collection_names_,
                             calo_collection_names_, gp_collection_names_);
}

// ---------------------------------------------------------------------------
// discoverCollections — find tracker/calo/GP names from the first source
// ---------------------------------------------------------------------------
void PodioROOTDataHandler::discoverCollections(
    const std::vector<std::unique_ptr<DataSource>>& sources)
{
    if (sources.empty()) {
        std::cout << "Warning: No sources for collection discovery" << std::endl;
        return;
    }

    auto* psrc = dynamic_cast<PodioEDM4hepDataSource*>(sources[0].get());
    if (!psrc) {
        std::cout << "Warning: First source is not a PodioEDM4hepDataSource" << std::endl;
        return;
    }

    // Let the source discover names by loading the first frame
    // getAvailableCollections() needs the source initialized first;
    // we use a minimal initialize so we can query names.
    psrc->initialize({}, {}, {});

    auto all_names = psrc->getAvailableCollections();

    for (const auto& n : all_names) {
        if (n.find("SimTrackerHit") != std::string::npos && n.find("_") != 0) {
            tracker_collection_names_.push_back(n);
        } else if (n.find("SimCalorimeterHit") != std::string::npos
                   && n.find("Contribution") == std::string::npos
                   && n.find("_") != 0) {
            calo_collection_names_.push_back(n);
        } else {
            static const std::array<const char*, 4> gp_patterns = {
                "GPIntKeys", "GPFloatKeys", "GPDoubleKeys", "GPStringKeys"
            };
            for (const auto* pat : gp_patterns) {
                if (n.find(pat) == 0) {
                    gp_collection_names_.push_back(n);
                    break;
                }
            }
        }
    }

    std::cout << "PodioROOTDataHandler discovered collections:" << std::endl;
    std::cout << "  Tracker: ";
    for (const auto& n : tracker_collection_names_) std::cout << n << " ";
    std::cout << "\n  Calo: ";
    for (const auto& n : calo_collection_names_) std::cout << n << " ";
    std::cout << "\n  GP: ";
    for (const auto& n : gp_collection_names_) std::cout << n << " ";
    std::cout << std::endl;

    // Initialize all sources with the discovered collection names
    for (auto& src : sources) {
        src->initialize(tracker_collection_names_, calo_collection_names_, gp_collection_names_);
    }
}

// ---------------------------------------------------------------------------
// processEvent — accumulate data from one source event into collections_
// (mirrors EDM4hepDataHandler::processEvent)
// ---------------------------------------------------------------------------
void PodioROOTDataHandler::processEvent(DataSource& source)
{
    auto* edm4hep_source = dynamic_cast<EDM4hepDataSource*>(&source);
    if (!edm4hep_source) {
        throw std::runtime_error("PodioROOTDataHandler: expected EDM4hepDataSource");
    }

    static int totalEventsConsumed = 0;

    size_t particle_index_offset     = collections_.mcparticles.size();
    size_t particle_parents_offset   = collections_.mcparticle_parents_refs.size();
    size_t particle_daughters_offset = collections_.mcparticle_daughters_refs.size();

    // MCParticles
    auto& proc_particles = edm4hep_source->processMCParticles(
        particle_parents_offset, particle_daughters_offset, totalEventsConsumed);
    collections_.mcparticles.insert(collections_.mcparticles.end(),
        std::make_move_iterator(proc_particles.begin()),
        std::make_move_iterator(proc_particles.end()));

    // MCParticle parent/daughter refs
    auto& proc_parents = edm4hep_source->processObjectID(
        "_MCParticles_parents", particle_index_offset, totalEventsConsumed);
    collections_.mcparticle_parents_refs.insert(collections_.mcparticle_parents_refs.end(),
        std::make_move_iterator(proc_parents.begin()),
        std::make_move_iterator(proc_parents.end()));

    auto& proc_daughters = edm4hep_source->processObjectID(
        "_MCParticles_daughters", particle_index_offset, totalEventsConsumed);
    collections_.mcparticle_daughters_refs.insert(collections_.mcparticle_daughters_refs.end(),
        std::make_move_iterator(proc_daughters.begin()),
        std::make_move_iterator(proc_daughters.end()));

    const auto& config = edm4hep_source->getConfig();

    // SubEventHeaders
    if (!config.already_merged) {
        edm4hep::EventHeaderData sub_hdr{};
        sub_hdr.eventNumber = static_cast<uint64_t>(totalEventsConsumed);
        sub_hdr.runNumber   = static_cast<uint32_t>(edm4hep_source->getSourceIndex());
        sub_hdr.timeStamp   = static_cast<uint64_t>(particle_index_offset);
        sub_hdr.weight      = edm4hep_source->getCurrentTimeOffset();
        collections_.sub_event_headers.push_back(sub_hdr);
        collections_.sub_event_header_weights.push_back(sub_hdr.weight);
    } else {
        auto& existing = edm4hep_source->processEventHeaders("SubEventHeaders");
        for (auto& sh : existing) {
            sh.weight += static_cast<float>(particle_index_offset);
            collections_.sub_event_headers.push_back(sh);
            collections_.sub_event_header_weights.push_back(sh.weight);
        }
    }

    // Tracker hits
    for (const auto& name : tracker_collection_names_) {
        auto& proc_hits = edm4hep_source->processTrackerHits(
            name, particle_index_offset, totalEventsConsumed);
        auto& dst = collections_.tracker_hits[name];
        dst.insert(dst.end(),
            std::make_move_iterator(proc_hits.begin()),
            std::make_move_iterator(proc_hits.end()));

        auto& proc_refs = edm4hep_source->processObjectID(
            "_" + name + "_particle", particle_index_offset, totalEventsConsumed);
        auto& dst_refs = collections_.tracker_hit_particle_refs[name];
        dst_refs.insert(dst_refs.end(),
            std::make_move_iterator(proc_refs.begin()),
            std::make_move_iterator(proc_refs.end()));
    }

    // Calo hits + contributions
    for (const auto& name : calo_collection_names_) {
        size_t existing_contrib_size = collections_.calo_contributions[name].size();

        auto& proc_hits = edm4hep_source->processCaloHits(
            name, existing_contrib_size, totalEventsConsumed);
        auto& dst_hits = collections_.calo_hits[name];
        dst_hits.insert(dst_hits.end(),
            std::make_move_iterator(proc_hits.begin()),
            std::make_move_iterator(proc_hits.end()));

        auto& proc_hit_refs = edm4hep_source->processObjectID(
            "_" + name + "_contributions", existing_contrib_size, totalEventsConsumed);
        auto& dst_hit_refs = collections_.calo_hit_contributions_refs[name];
        dst_hit_refs.insert(dst_hit_refs.end(),
            std::make_move_iterator(proc_hit_refs.begin()),
            std::make_move_iterator(proc_hit_refs.end()));

        std::string contrib_name = name + "Contributions";
        auto& proc_contribs = edm4hep_source->processCaloContributions(
            contrib_name, particle_index_offset, totalEventsConsumed);
        auto& dst_contribs = collections_.calo_contributions[name];
        dst_contribs.insert(dst_contribs.end(),
            std::make_move_iterator(proc_contribs.begin()),
            std::make_move_iterator(proc_contribs.end()));

        auto& proc_contrib_refs = edm4hep_source->processObjectID(
            "_" + contrib_name + "_particle", particle_index_offset, totalEventsConsumed);
        auto& dst_contrib_refs = collections_.calo_contrib_particle_refs[name];
        dst_contrib_refs.insert(dst_contrib_refs.end(),
            std::make_move_iterator(proc_contrib_refs.begin()),
            std::make_move_iterator(proc_contrib_refs.end()));
    }

    // GP branches
    for (const auto& name : gp_collection_names_) {
        auto& gp_keys = edm4hep_source->processGPBranch(name);
        auto& dst = collections_.gp_key_branches[name];
        dst.insert(dst.end(),
            std::make_move_iterator(gp_keys.begin()),
            std::make_move_iterator(gp_keys.end()));
    }

    auto& gp_int = edm4hep_source->processGPIntValues();
    collections_.gp_int_values.insert(collections_.gp_int_values.end(),
        std::make_move_iterator(gp_int.begin()), std::make_move_iterator(gp_int.end()));

    auto& gp_float = edm4hep_source->processGPFloatValues();
    collections_.gp_float_values.insert(collections_.gp_float_values.end(),
        std::make_move_iterator(gp_float.begin()), std::make_move_iterator(gp_float.end()));

    auto& gp_double = edm4hep_source->processGPDoubleValues();
    collections_.gp_double_values.insert(collections_.gp_double_values.end(),
        std::make_move_iterator(gp_double.begin()), std::make_move_iterator(gp_double.end()));

    auto& gp_string = edm4hep_source->processGPStringValues();
    collections_.gp_string_values.insert(collections_.gp_string_values.end(),
        std::make_move_iterator(gp_string.begin()), std::make_move_iterator(gp_string.end()));

    ++totalEventsConsumed;
}
