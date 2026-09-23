#include "EDM4hepDataHandler.h"
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <TBranch.h>
#include <TObjArray.h>
#include <TChain.h>

void EDM4hepMergedCollections::clear() {
    // Use clear() but preserve capacity to avoid repeated memory allocations
    mcparticles.clear();
    event_headers.clear();
    event_header_weights.clear();
    sub_event_headers.clear();
    sub_event_header_weights.clear();
    
    for (auto& [name, vec] : tracker_hits) {
        vec.clear();
    }
    for (auto& [name, vec] : calo_hits) {
        vec.clear();
    }
    for (auto& [name, vec] : calo_contributions) {
        vec.clear();
    }
    for (auto& [name, vec] : tracker_hit_particle_refs) {
        vec.clear();
    }
    for (auto& [name, vec] : calo_contrib_particle_refs) {
        vec.clear();
    }
    
    mcparticle_parents_refs.clear();
    mcparticle_daughters_refs.clear();
    
    for (auto& [name, vec] : calo_hit_contributions_refs) {
        vec.clear();
    }
    
    // Clear GP branches
    for (auto& [name, vec] : gp_key_branches) {
        vec.clear();
    }
    gp_int_values.clear();
    gp_float_values.clear();
    gp_double_values.clear();
    gp_string_values.clear();
}

std::vector<std::unique_ptr<DataSource>> EDM4hepDataHandler::initializeDataSources(
    const std::string& filename,
    const std::vector<SourceConfig>& source_configs) {
    
    std::cout << "Initializing EDM4hep data handler for: " << filename << std::endl;
    
    std::vector<std::unique_ptr<DataSource>> data_sources;
    data_sources.reserve(source_configs.size());
    
    // Create EDM4hepDataSource objects for each source configuration
    for (size_t source_idx = 0; source_idx < source_configs.size(); ++source_idx) {
        const auto& source_config = source_configs[source_idx];
        
        // Determine format from first input file
        if (source_config.input_files.empty()) {
            throw std::runtime_error("Source " + source_config.name + " has no input files");
        }
        
        const std::string& first_file = source_config.input_files[0];
        
        // Helper lambda to check file extension
        auto hasExtension = [](const std::string& filename, const std::string& ext) {
            if (filename.length() < ext.length()) return false;
            return filename.compare(filename.length() - ext.length(), ext.length(), ext) == 0;
        };
        
        // Check if this is EDM4hep format
        if (!hasExtension(first_file, ".edm4hep.root") && !hasExtension(first_file, ".root")) {
            throw std::runtime_error(
                "EDM4hepDataHandler can only handle .edm4hep.root or .root files. "
                "Got: " + first_file
            );
        }
        
        auto data_source = std::make_unique<EDM4hepDataSource>(source_config, source_idx);
        std::cout << "Created EDM4hepDataSource for: " << first_file << std::endl;
        data_sources.push_back(std::move(data_source));
    }
    
    // Store pointers to EDM4hep sources for later use
    edm4hep_sources_.clear();
    edm4hep_sources_.reserve(data_sources.size());
    for (auto& source : data_sources) {
        edm4hep_sources_.push_back(dynamic_cast<EDM4hepDataSource*>(source.get()));
    }
    
    // Open output file
    output_file_ = std::make_unique<TFile>(filename.c_str(), "RECREATE");
    if (!output_file_ || output_file_->IsZombie()) {
        throw std::runtime_error("Could not create output file: " + filename);
    }
    
    std::cout << "EDM4hep data handler initialized successfully" << std::endl;
    
    return data_sources;
}

void EDM4hepDataHandler::initializeOutput(const MergerConfig& config, const std::vector<std::unique_ptr<DataSource>>& data_sources) {
    // Open output file
    output_file_ = std::make_unique<TFile>(config.output_file.c_str(), "RECREATE");
    if (!output_file_ || output_file_->IsZombie()) {
        throw std::runtime_error("Could not create output file: " + config.output_file);
    }
    
    // Set ROOT I/O optimizations
    output_file_->SetCompressionLevel(1);
    
    // Create output tree
    output_tree_ = new TTree("events", "Merged timeframes");
    
    // Discover collections from sources
    discoverCollections(data_sources);
    
    // Setup output tree branches
    setupOutputTree();
    
    // Copy metadata from first source
    copyPodioMetadata(data_sources);
    
    std::cout << "EDM4hep data handler initialized successfully" << std::endl;
}

void EDM4hepDataHandler::prepareTimeframe() {
    collections_.clear();
    // Restore the output file as the current ROOT directory so that any ROOT
    // book-keeping triggered by subsequent readNextEntry() / loadEvent() calls
    // (which open input files and change gDirectory) does not corrupt the output
    // tree's internal state before writeTimeframe() / Fill() is called.
    if (output_file_) output_file_->cd();
}

void EDM4hepDataHandler::processEvent(DataSource& source) {
    auto* edm4hep_source = dynamic_cast<EDM4hepDataSource*>(&source);
    if (!edm4hep_source) {
        throw std::runtime_error("EDM4hepDataHandler: Expected EDM4hepDataSource");
    }
    
    static int totalEventsConsumed = 0;  // Track across all sources
    
    // Calculate particle index offset for this event
    size_t particle_index_offset   = collections_.mcparticles.size();
    size_t particle_parents_offset = collections_.mcparticle_parents_refs.size();
    size_t particle_daughters_offset = collections_.mcparticle_daughters_refs.size();
    
    // Process MCParticles - use move semantics to avoid copying
    auto& processed_particles = edm4hep_source->processMCParticles(particle_parents_offset, particle_daughters_offset, totalEventsConsumed);
    collections_.mcparticles.insert(collections_.mcparticles.end(), 
                                          std::make_move_iterator(processed_particles.begin()), 
                                          std::make_move_iterator(processed_particles.end()));
    
    // Process MCParticle references - use move semantics
    std::string parent_ref_branch_name = "_MCParticles_parents";
    auto& processed_parents = edm4hep_source->processObjectID(parent_ref_branch_name, particle_index_offset,totalEventsConsumed);
    collections_.mcparticle_parents_refs.insert(collections_.mcparticle_parents_refs.end(),
                                                      std::make_move_iterator(processed_parents.begin()), 
                                                      std::make_move_iterator(processed_parents.end()));

    std::string daughters_ref_branch_name = "_MCParticles_daughters";
    auto& processed_daughters = edm4hep_source->processObjectID(daughters_ref_branch_name, particle_index_offset,totalEventsConsumed);
    collections_.mcparticle_daughters_refs.insert(collections_.mcparticle_daughters_refs.end(),
                                                       std::make_move_iterator(processed_daughters.begin()), 
                                                       std::make_move_iterator(processed_daughters.end()));

    const auto& config = edm4hep_source->getConfig();
    
    // Process SubEventHeaders for non-merged sources to track which MCParticles came from this source
    if (!config.already_merged) {
        // Create a SubEventHeader for this source/event combination
        edm4hep::EventHeaderData sub_header;
        sub_header.eventNumber = totalEventsConsumed;
        sub_header.runNumber = edm4hep_source->getSourceIndex();
        sub_header.timeStamp = particle_index_offset;
        sub_header.weight = edm4hep_source->getCurrentTimeOffset();

        collections_.sub_event_headers.push_back(sub_header);
        collections_.sub_event_header_weights.push_back(sub_header.weight);
    } else {
        // For already merged sources, process existing SubEventHeaders if available
        auto& existing_sub_headers = edm4hep_source->processEventHeaders("SubEventHeaders");
        for (auto& sub_header : existing_sub_headers) {
            float original_offset = sub_header.weight;
            sub_header.weight += static_cast<float>(particle_index_offset);
            collections_.sub_event_headers.push_back(sub_header);
            collections_.sub_event_header_weights.push_back(sub_header.weight);
        }
    }
    
    // Process tracker hits
    for (const auto& name : tracker_collection_names_) {
        auto& processed_hits = edm4hep_source->processTrackerHits(name, particle_index_offset,totalEventsConsumed);
        collections_.tracker_hits[name].insert(collections_.tracker_hits[name].end(),
                                                     std::make_move_iterator(processed_hits.begin()), 
                                                     std::make_move_iterator(processed_hits.end()));

        std::string ref_branch_name = "_" + name + "_particle";
        auto& processed_refs = edm4hep_source->processObjectID(ref_branch_name, particle_index_offset,totalEventsConsumed);
        collections_.tracker_hit_particle_refs[name].insert(collections_.tracker_hit_particle_refs[name].end(),
                                                                  std::make_move_iterator(processed_refs.begin()), 
                                                                  std::make_move_iterator(processed_refs.end()));
    }
    
    // Process calorimeter hits
    for (const auto& name : calo_collection_names_) {
        size_t existing_contrib_size = collections_.calo_contributions[name].size();

        auto& processed_hits = edm4hep_source->processCaloHits(name, existing_contrib_size,totalEventsConsumed);
        collections_.calo_hits[name].insert(collections_.calo_hits[name].end(),
                                                  std::make_move_iterator(processed_hits.begin()), 
                                                  std::make_move_iterator(processed_hits.end()));
        
        std::string ref_branch_name = "_" + name + "_contributions";
        auto& processed_contrib_refs = edm4hep_source->processObjectID(ref_branch_name, existing_contrib_size,totalEventsConsumed);
        collections_.calo_hit_contributions_refs[name].insert(collections_.calo_hit_contributions_refs[name].end(),
                                                                    std::make_move_iterator(processed_contrib_refs.begin()), 
                                                                    std::make_move_iterator(processed_contrib_refs.end()));
        
        // Process contributions
        std::string contrib_branch_name = name + "Contributions";
        auto& processed_contribs = edm4hep_source->processCaloContributions(contrib_branch_name, particle_index_offset,totalEventsConsumed);
        collections_.calo_contributions[name].insert(collections_.calo_contributions[name].end(),
                                                           std::make_move_iterator(processed_contribs.begin()), 
                                                           std::make_move_iterator(processed_contribs.end()));
        
        std::string ref_branch_name_contrib = "_" + contrib_branch_name + "_particle";
        auto& processed_contrib_particle_refs = edm4hep_source->processObjectID(ref_branch_name_contrib, particle_index_offset,totalEventsConsumed);
        collections_.calo_contrib_particle_refs[name].insert(collections_.calo_contrib_particle_refs[name].end(),
                                                                   std::make_move_iterator(processed_contrib_particle_refs.begin()), 
                                                                   std::make_move_iterator(processed_contrib_particle_refs.end()));
    }
    
    // Process GP (Global Parameter) branches
    for (const auto& name : gp_collection_names_) {
        auto& gp_keys = edm4hep_source->processGPBranch(name);
        collections_.gp_key_branches[name].insert(collections_.gp_key_branches[name].end(),
                                                            std::make_move_iterator(gp_keys.begin()), std::make_move_iterator(gp_keys.end()));
    }

    // Process GP value branches
    auto& gp_int_values = edm4hep_source->processGPIntValues();
    collections_.gp_int_values.insert(collections_.gp_int_values.end(),
        std::make_move_iterator(gp_int_values.begin()), std::make_move_iterator(gp_int_values.end()));

    auto& gp_float_values = edm4hep_source->processGPFloatValues();
    collections_.gp_float_values.insert(collections_.gp_float_values.end(),
        std::make_move_iterator(gp_float_values.begin()), std::make_move_iterator(gp_float_values.end()));

    auto& gp_double_values = edm4hep_source->processGPDoubleValues();
    collections_.gp_double_values.insert(collections_.gp_double_values.end(),
        std::make_move_iterator(gp_double_values.begin()), std::make_move_iterator(gp_double_values.end()));

    auto& gp_string_values = edm4hep_source->processGPStringValues();
    collections_.gp_string_values.insert(collections_.gp_string_values.end(),
        std::make_move_iterator(gp_string_values.begin()), std::make_move_iterator(gp_string_values.end()));

    totalEventsConsumed++;

    // After each loadEvent() + processEvent() cycle, restore the output file as
    // the current ROOT directory.  readNextEntry()/readEntry() opens input files
    // via ROOT's file manager and updates gDirectory; if ROOT's internal basket
    // bookkeeping runs between mergeEvents() iterations with the wrong gDirectory
    // it will misdirect I/O and cause catastrophically slow writes.
    if (output_file_) output_file_->cd();
}

void EDM4hepDataHandler::writeTimeframe() {
    if (!output_tree_) {
        throw std::runtime_error("Output tree not initialized");
    }
    
    // Create main timeframe header
    edm4hep::EventHeaderData header;
    header.eventNumber = current_timeframe_number_;
    header.runNumber = 0;
    header.timeStamp = current_timeframe_number_;
    collections_.event_headers.push_back(header);

    // Restore the output file as the current ROOT directory before Fill().
    // Multiple podio ROOTReader instances each open their input files via
    // ROOT's file manager, which updates gDirectory. Without this cd() call,
    // TBranchElement::WriteBasketImpl fails when it can't resolve the write
    // destination for the output TTree.
    output_file_->cd();
    output_tree_->Fill();
    std::cout << "=== Timeframe written ===" << std::endl;
}

void EDM4hepDataHandler::finalize() {
    if (output_file_) {
        output_file_->cd();
        if (output_tree_) {
            output_tree_->Write();
        }
        
        // Write all other objects (metadata trees) that are in memory
        output_file_->Write(nullptr, TObject::kOverwrite);
        output_file_->Close();
    }
    std::cout << "EDM4hep output finalized" << std::endl;
}

void EDM4hepDataHandler::setupOutputTree() {
    if (!output_tree_) {
        throw std::runtime_error("Cannot setup output tree - tree is null");
    }
    
    // Create all required branches
    output_tree_->Branch("EventHeader", &collections_.event_headers);
    output_tree_->Branch("_EventHeader_weights", &collections_.event_header_weights);
    output_tree_->Branch("SubEventHeaders", &collections_.sub_event_headers);
    output_tree_->Branch("_SubEventHeader_weights", &collections_.sub_event_header_weights);
    output_tree_->Branch("MCParticles", &collections_.mcparticles);
    output_tree_->Branch("_MCParticles_daughters", &collections_.mcparticle_daughters_refs);
    output_tree_->Branch("_MCParticles_parents", &collections_.mcparticle_parents_refs);

    // Tracker collections and their references
    for (const auto& name : tracker_collection_names_) {
        output_tree_->Branch(name.c_str(), &collections_.tracker_hits[name]);        
        std::string ref_name = "_" + name + "_particle";
        output_tree_->Branch(ref_name.c_str(), &collections_.tracker_hit_particle_refs[name]);
    }

    // Calorimeter collections and their references
    for (const auto& name : calo_collection_names_) {
        output_tree_->Branch(name.c_str(), &collections_.calo_hits[name]);        
        std::string ref_name = "_" + name + "_contributions";
        output_tree_->Branch(ref_name.c_str(), &collections_.calo_hit_contributions_refs[name]);        
        std::string contrib_name = name + "Contributions";
        output_tree_->Branch(contrib_name.c_str(), &collections_.calo_contributions[name]);        
        std::string ref_name_contrib = "_" + contrib_name + "_particle";
        output_tree_->Branch(ref_name_contrib.c_str(), &collections_.calo_contrib_particle_refs[name]);
    }
    
    // GP (Global Parameter) branches
    for (const auto& name : gp_collection_names_) {
        output_tree_->Branch(name.c_str(), &collections_.gp_key_branches[name]);
    }
    
    output_tree_->Branch("GPIntValues", &collections_.gp_int_values);    
    output_tree_->Branch("GPFloatValues", &collections_.gp_float_values);    
    output_tree_->Branch("GPDoubleValues", &collections_.gp_double_values);    
    output_tree_->Branch("GPStringValues", &collections_.gp_string_values);
    
    std::cout << "Total branches created: " << output_tree_->GetListOfBranches()->GetEntries() << std::endl;
}

void EDM4hepDataHandler::discoverCollections(const std::vector<std::unique_ptr<DataSource>>& sources) {
    if (sources.empty() || sources[0]->getConfig().input_files.empty()) {
        std::cout << "Warning: No sources available for collection discovery" << std::endl;
        return;
    }
    
    tracker_collection_names_ = discoverCollectionNames(*sources[0], "SimTrackerHit");
    calo_collection_names_ = discoverCollectionNames(*sources[0], "SimCalorimeterHit");
    gp_collection_names_ = discoverGPBranches(*sources[0]);
    
    std::cout << "EDM4hep collection names discovered:" << std::endl;
    std::cout << "  Tracker: ";
    for (const auto& name : tracker_collection_names_) std::cout << name << " ";
    std::cout << std::endl;
    std::cout << "  Calo: ";
    for (const auto& name : calo_collection_names_) std::cout << name << " ";
    std::cout << std::endl;
    std::cout << "  GP: ";
    for (const auto& name : gp_collection_names_) std::cout << name << " ";
    std::cout << std::endl;
    
    // Initialize all data sources with discovered collection names
    for (const auto& edm4hep_source : sources) {
        const_cast<DataSource&>(*edm4hep_source).initialize(tracker_collection_names_, calo_collection_names_, gp_collection_names_);
    }
}

std::vector<std::string> EDM4hepDataHandler::discoverCollectionNames(DataSource& source, const std::string& branch_pattern) {
    std::vector<std::string> names;
    
    if (source.getConfig().input_files.empty()) {
        return names;
    }
    
    auto temp_chain = std::make_unique<TChain>(source.getConfig().tree_name.c_str());
    temp_chain->Add(source.getConfig().input_files[0].c_str());
    
    TObjArray* branches = temp_chain->GetListOfBranches();
    if (!branches) {
        return names;
    }
    
    for (int i = 0; i < branches->GetEntries(); ++i) {
        TBranch* branch = (TBranch*)branches->At(i);
        if (!branch) continue;
        
        std::string branch_name = branch->GetName();
        std::string branch_type = "";
        
        TClass* expectedClass = nullptr;
        EDataType expectedType;
        if (branch->GetExpectedType(expectedClass, expectedType) == 0 && expectedClass && expectedClass->GetName()) {
            branch_type = expectedClass->GetName();
        }
        
        if (branch_name.find("_") == 0) continue;
        if (branch_type.find(branch_pattern) == std::string::npos) continue;
        
        if (branch_type == "vector<edm4hep::SimTrackerHitData>" || 
            branch_type == "vector<edm4hep::SimCalorimeterHitData>") {
            names.push_back(branch_name);
        }
    }
    
    return names;
}

std::vector<std::string> EDM4hepDataHandler::discoverGPBranches(DataSource& source) {
    std::vector<std::string> names;
    
    if (source.getConfig().input_files.empty()) {
        return names;
    }
    
    auto temp_chain = std::make_unique<TChain>(source.getConfig().tree_name.c_str());
    temp_chain->Add(source.getConfig().input_files[0].c_str());
    
    TObjArray* branches = temp_chain->GetListOfBranches();
    if (!branches) {
        return names;
    }
    
    std::vector<std::string> gp_patterns = {"GPIntKeys", "GPFloatKeys", "GPStringKeys", "GPDoubleKeys"};

    for (int i = 0; i < branches->GetEntries(); ++i) {
        TBranch* branch = (TBranch*)branches->At(i);
        if (!branch) continue;
        
        std::string branch_name = branch->GetName();
        
        for (const auto& pattern : gp_patterns) {
            if (branch_name.find(pattern) == 0) {
                names.push_back(branch_name);
                break;
            }
        }
    }
    
    return names;
}

void EDM4hepDataHandler::copyPodioMetadata(const std::vector<std::unique_ptr<DataSource>>& sources) {
    if (sources.empty() || !output_file_) {
        std::cout << "Warning: Cannot copy PODIO metadata - no sources or output file" << std::endl;
        return;
    }
    
    const auto& first_source = sources[0];
    if (first_source->getConfig().input_files.empty()) {
        std::cout << "Warning: Cannot copy PODIO metadata - no input files in first source" << std::endl;
        return;
    }
    
    std::string first_file = first_source->getConfig().input_files[0];
    std::cout << "Copying PODIO metadata from: " << first_file << std::endl;
    auto source_file = std::unique_ptr<TFile>{TFile::Open(first_file.c_str(), "READ")};
    if (!source_file || source_file->IsZombie()) {
        std::cout << "Warning: Cannot open source file for metadata: " << first_file << std::endl;
        return;
    }
    
    output_file_->cd();
    
    std::vector<std::string> metadata_trees = {"podio_metadata", "runs", "meta", "metadata"};
    
    for (const auto& tree_name : metadata_trees) {
        TTree* metadata_tree = dynamic_cast<TTree*>(source_file->Get(tree_name.c_str()));
        if (metadata_tree) {
            std::cout << "Found metadata tree: " << tree_name << std::endl;
            if (tree_name == "podio_metadata") {
                copyAndUpdatePodioMetadataTree(metadata_tree, output_file_.get());
            } else {
                TTree* output_metadata = metadata_tree->CloneTree(-1, "fast");
                if (output_metadata) {
                    output_metadata->SetDirectory(output_file_.get());
                    std::cout << "Successfully cloned metadata tree: " << tree_name << std::endl;
                }
            }
        }
    }
    
    source_file->Close();
}

void EDM4hepDataHandler::copyAndUpdatePodioMetadataTree(TTree* source_metadata_tree, TFile* output_file) {
    if (!source_metadata_tree || !output_file) {
        std::cout << "Warning: Cannot copy podio_metadata tree - null pointer" << std::endl;
        return;
    }
    
    TTree* output_metadata = source_metadata_tree->CloneTree(-1, "fast");
    if (output_metadata) {
        output_metadata->SetDirectory(output_file);
        std::cout << "Successfully copied podio_metadata tree with " << output_metadata->GetEntries() << " entries" << std::endl;
    } else {
        std::cout << "Warning: Failed to clone podio_metadata tree" << std::endl;
    }
}

std::string EDM4hepDataHandler::getCorrespondingContributionCollection(const std::string& calo_collection_name) const {
    return calo_collection_name + "Contributions";
}

std::string EDM4hepDataHandler::getCorrespondingCaloCollection(const std::string& contrib_collection_name) const {
    std::string base = contrib_collection_name;
    if (base.length() > 13 && base.substr(base.length() - 13) == "Contributions") {
        base = base.substr(0, base.length() - 13);
    }
    return base;
}
