#include "EDM4hepDataSource.h"
#include <TBranch.h>
#include <TObjArray.h>
#include <algorithm>
#include <cmath>
#include <iostream>

EDM4hepDataSource::EDM4hepDataSource(const SourceConfig &config,
                                     size_t source_index)
    : tracker_collection_names_(nullptr), calo_collection_names_(nullptr),
      mcparticle_branch_(nullptr), gp_int_branch_(nullptr),
      gp_float_branch_(nullptr), gp_double_branch_(nullptr),
      gp_string_branch_(nullptr), current_particle_index_offset_(0) {
  config_ = &config;
  source_index_ = source_index;
  total_entries_ = 0;
  current_entry_index_ = 0;
  entries_needed_ = 1;
}

EDM4hepDataSource::~EDM4hepDataSource() { cleanup(); }

void EDM4hepDataSource::initialize(
    const std::vector<std::string> &tracker_collections,
    const std::vector<std::string> &calo_collections,
    const std::vector<std::string> &gp_collections) {
  // Store references to collection names
  tracker_collection_names_ = &tracker_collections;
  calo_collection_names_ = &calo_collections;
  gp_collection_names_ = &gp_collections;

  if (!config_->input_files.empty()) {
    try {
      // Create TChain for this source
      chain_ = std::make_unique<TChain>(config_->tree_name.c_str());

      // Add all input files to the chain
      for (const auto &file : config_->input_files) {
        int result = chain_->Add(file.c_str());
        if (result == 0) {
          throw std::runtime_error("Failed to add file: " + file);
        }
        std::cout << "Added file to source " << source_index_ << ": " << file
                  << std::endl;
      }

      total_entries_ = chain_->GetEntries();

      if (total_entries_ == 0) {
        throw std::runtime_error("No entries found in source " +
                                 std::to_string(source_index_));
      }

      std::cout << "Source " << source_index_ << " has " << total_entries_
                << " entries" << std::endl;

      // Skip initial events if specified
      if (config_->skip > 0) {
        if (config_->skip >= total_entries_ && !config_->repeat_on_eof) {
          throw std::runtime_error("Skip value exceeds total entries and "
                                   "repeat_on_eof is false for source " +
                                   std::to_string(source_index_));
        }
        current_entry_index_ =
            config_->skip %
            total_entries_; // Wrap around if repeat_on_eof is true
        std::cout << "Skipping first " << config_->skip << " events for source "
                  << source_index_ << ", starting at entry "
                  << current_entry_index_ << std::endl;
      }

      // Setup branch addresses
      setupBranches();

      std::cout << "Successfully initialized EDM4hep source " << source_index_
                << " (" << config_->name << ")" << std::endl;

    } catch (const std::exception &e) {
      throw std::runtime_error("ERROR: Could not open input files for source " +
                               std::to_string(source_index_) + ": " + e.what());
    }
  }
}

bool EDM4hepDataSource::hasMoreEntries() const {
  if (config_->repeat_on_eof && total_entries_ > 0) {
    return true;
  }
  return (current_entry_index_ + entries_needed_) <= total_entries_;
}

bool EDM4hepDataSource::loadNextEvent() {
  if (current_entry_index_ >= total_entries_) {
    if (config_->repeat_on_eof) {
      current_entry_index_ = 0;
    }
    return false;
  }

  chain_->GetEntry(current_entry_index_);
  return true;
}

void EDM4hepDataSource::loadEvent(size_t event_index) {
  chain_->GetEntry(event_index);
}

std::vector<podio::ObjectID> &
EDM4hepDataSource::processObjectID(const std::string &branch_name,
                                   size_t index_offset,
                                   int totalEventsConsumed) {

  // If first event and already merged, skip updating references
  if (totalEventsConsumed == 0 && config_->already_merged) {
    return *objectid_branches_[branch_name];
  }

  // Update references with index offset
  for (auto &ref : *objectid_branches_[branch_name]) {
    ref.index += index_offset;
  }

  return *objectid_branches_[branch_name];
}

std::vector<edm4hep::MCParticleData> &
EDM4hepDataSource::processMCParticles(size_t particle_parents_offset,
                                      size_t particle_daughters_offset,
                                      int totalEventsConsumed) {

  auto &particles = *mcparticle_branch_;

  if (totalEventsConsumed == 0 && config_->already_merged) {
    return particles;
  }

  float distance = 0.0f;

  // Work directly on the branch data
  for (auto &particle : particles) {
    if (!config_->already_merged) {
      particle.time += current_time_offset_;
      // Update generator status offset
      particle.generatorStatus += config_->generator_status_offset;
    }
    // Update index ranges for parent-child relationships
    particle.parents_begin += particle_parents_offset;
    particle.parents_end += particle_parents_offset;
    particle.daughters_begin += particle_daughters_offset;
    particle.daughters_end += particle_daughters_offset;
  }

  return particles; // Return reference to the branch data itself
}

std::vector<edm4hep::SimTrackerHitData> &
EDM4hepDataSource::processTrackerHits(const std::string &collection_name,
                                      size_t particle_index_offset,
                                      int totalEventsConsumed) {

  if (totalEventsConsumed == 0 && config_->already_merged) {
    return *tracker_hit_branches_[collection_name];
  }

  // Apply index offset to particle references in hits
  if (!config_->already_merged) {
    for (auto &hit :
         *tracker_hit_branches_[collection_name]) { // Apply time offset if not
                                                    // already merged
      hit.time += current_time_offset_;
    }
  }

  return *tracker_hit_branches_[collection_name]; // Return reference to the
                                                  // branch data itself
}

std::vector<edm4hep::SimCalorimeterHitData> &
EDM4hepDataSource::processCaloHits(const std::string &collection_name,
                                   size_t contribution_index_offset,
                                   int totalEventsConsumed) {

  if (totalEventsConsumed == 0 && config_->already_merged) {
    return *calo_hit_branches_[collection_name];
  }

  auto &hits = *calo_hit_branches_[collection_name];

  for (auto &hit : hits) {
    hit.contributions_begin += contribution_index_offset;
    hit.contributions_end += contribution_index_offset;
  }

  return hits; // Return reference to the branch data itself
}

std::vector<edm4hep::CaloHitContributionData> &
EDM4hepDataSource::processCaloContributions(const std::string &collection_name,
                                            size_t particle_index_offset,
                                            int totalEventsConsumed) {

  auto &contribs = *calo_contrib_branches_[collection_name];

  if (totalEventsConsumed == 0 && config_->already_merged) {
    return contribs;
  }

  // Apply time offset if not already merged - work directly on branch data
  if (!config_->already_merged) {
    for (auto &contrib : contribs) {
      contrib.time += current_time_offset_;
    }
  }

  return contribs; // Return reference to the branch data itself
}

std::vector<edm4hep::EventHeaderData> &
EDM4hepDataSource::processEventHeaders(const std::string &collection_name) {
  // Check if the collection exists in our event header branches
  if (event_header_branches_.find(collection_name) ==
      event_header_branches_.end()) {
    // Collection not found, return empty vector
    static std::vector<edm4hep::EventHeaderData> empty_headers;
    empty_headers.clear();
    return empty_headers;
  }

  // Get the current event headers
  auto *headers = event_header_branches_[collection_name];
  if (!headers) {
    static std::vector<edm4hep::EventHeaderData> empty_headers;
    empty_headers.clear();
    return empty_headers;
  }

  // Return reference to the branch data itself - no processing needed for
  // headers
  return *headers;
}

void EDM4hepDataSource::setupBranches() {
  std::cout << "=== Setting up EDM4hep branches for source " << source_index_
            << " ===" << std::endl;

  setupMCParticleBranches();
  setupTrackerBranches();
  setupCalorimeterBranches();
  setupEventHeaderBranches();
  setupGPBranches();

  std::cout << "=== EDM4hep branch setup complete ===" << std::endl;
}

void EDM4hepDataSource::setupMCParticleBranches() {
  // Setup MCParticles branch
  mcparticle_branch_ = new std::vector<edm4hep::MCParticleData>();
  int result = chain_->SetBranchAddress("MCParticles", &mcparticle_branch_);

  // Setup MCParticle parent-child relationship branches using consolidated map
  std::string parents_branch_name = "_MCParticles_parents";
  std::string children_branch_name = "_MCParticles_daughters";

  objectid_branches_[parents_branch_name] = new std::vector<podio::ObjectID>();
  objectid_branches_[children_branch_name] = new std::vector<podio::ObjectID>();

  result = chain_->SetBranchAddress(parents_branch_name.c_str(),
                                    &objectid_branches_[parents_branch_name]);
  result = chain_->SetBranchAddress(children_branch_name.c_str(),
                                    &objectid_branches_[children_branch_name]);
}

void EDM4hepDataSource::setupTrackerBranches() {
  for (const auto &coll_name : *tracker_collection_names_) {
    tracker_hit_branches_[coll_name] =
        new std::vector<edm4hep::SimTrackerHitData>();
    int result = chain_->SetBranchAddress(coll_name.c_str(),
                                          &tracker_hit_branches_[coll_name]);

    // Also setup the particle reference branch using consolidated map
    std::string ref_branch_name = "_" + coll_name + "_particle";
    objectid_branches_[ref_branch_name] = new std::vector<podio::ObjectID>();
    result = chain_->SetBranchAddress(ref_branch_name.c_str(),
                                      &objectid_branches_[ref_branch_name]);
  }
}

void EDM4hepDataSource::setupCalorimeterBranches() {
  // Setup calorimeter hit branches
  for (const auto &coll_name : *calo_collection_names_) {
    calo_hit_branches_[coll_name] =
        new std::vector<edm4hep::SimCalorimeterHitData>();
    int result = chain_->SetBranchAddress(coll_name.c_str(),
                                          &calo_hit_branches_[coll_name]);

    // Also setup the contributions reference branch using consolidated map
    std::string contrib_link_branch_name = "_" + coll_name + "_contributions";
    objectid_branches_[contrib_link_branch_name] =
        new std::vector<podio::ObjectID>();
    result =
        chain_->SetBranchAddress(contrib_link_branch_name.c_str(),
                                 &objectid_branches_[contrib_link_branch_name]);

    std::string contrib_branch_name = coll_name + "Contributions";
    calo_contrib_branches_[contrib_branch_name] =
        new std::vector<edm4hep::CaloHitContributionData>();
    result =
        chain_->SetBranchAddress(contrib_branch_name.c_str(),
                                 &calo_contrib_branches_[contrib_branch_name]);

    // Also setup the particle reference branch using consolidated map
    std::string ref_branch_name = "_" + contrib_branch_name + "_particle";
    objectid_branches_[ref_branch_name] = new std::vector<podio::ObjectID>();
    result = chain_->SetBranchAddress(ref_branch_name.c_str(),
                                      &objectid_branches_[ref_branch_name]);
  }
}

void EDM4hepDataSource::setupEventHeaderBranches() {
  // Setup EventHeader if available
  std::vector<std::string> header_collections = {"EventHeader"};

  // Only add SubEventHeaders for non-merged sources (where they aren't already
  // present)
  if (!config_->already_merged) {
    header_collections.push_back("SubEventHeaders");
  }

  for (const auto &coll_name : header_collections) {
    event_header_branches_[coll_name] =
        new std::vector<edm4hep::EventHeaderData>();
    int result = chain_->SetBranchAddress(coll_name.c_str(),
                                          &event_header_branches_[coll_name]);
  }
}

void EDM4hepDataSource::setupGPBranches() {
  // Initialize GP value branch pointers
  gp_int_branch_ = new std::vector<std::vector<int>>();
  gp_float_branch_ = new std::vector<std::vector<float>>();
  gp_double_branch_ = new std::vector<std::vector<double>>();
  gp_string_branch_ = new std::vector<std::vector<std::string>>();

  // Setup the fixed GP value branches
  int result = chain_->SetBranchAddress("GPIntValues", &gp_int_branch_);
  result = chain_->SetBranchAddress("GPFloatValues", &gp_float_branch_);
  result = chain_->SetBranchAddress("GPDoubleValues", &gp_double_branch_);
  result = chain_->SetBranchAddress("GPStringValues", &gp_string_branch_);

  // Setup GP key branches
  for (const auto &branch_name : *gp_collection_names_) {
    gp_key_branches_[branch_name] = new std::vector<std::string>();
    result = chain_->SetBranchAddress(branch_name.c_str(),
                                      &gp_key_branches_[branch_name]);
  }
}

std::vector<std::string> &
EDM4hepDataSource::processGPBranch(const std::string &branch_name) {
  // GP key branches don't need any processing, just return the data as-is
  // They contain global parameter keys that should be copied unchanged
  return *gp_key_branches_[branch_name];
}

std::vector<std::vector<int>> &EDM4hepDataSource::processGPIntValues() {
  // GP int values don't need any processing, just return the data as-is
  return *gp_int_branch_;
}

std::vector<std::vector<float>> &EDM4hepDataSource::processGPFloatValues() {
  // GP float values don't need any processing, just return the data as-is
  return *gp_float_branch_;
}

std::vector<std::vector<double>> &EDM4hepDataSource::processGPDoubleValues() {
  // GP double values don't need any processing, just return the data as-is
  return *gp_double_branch_;
}

std::vector<std::vector<std::string>> &
EDM4hepDataSource::processGPStringValues() {
  // GP string values don't need any processing, just return the data as-is
  return *gp_string_branch_;
}

void EDM4hepDataSource::cleanup() {
  // Clean up dynamically allocated vectors
  delete mcparticle_branch_;

  for (auto &[name, ptr] : tracker_hit_branches_) {
    delete ptr;
  }
  for (auto &[name, ptr] : calo_hit_branches_) {
    delete ptr;
  }
  for (auto &[name, ptr] : calo_contrib_branches_) {
    delete ptr;
  }
  for (auto &[name, ptr] : event_header_branches_) {
    delete ptr;
  }
  for (auto &[name, ptr] : objectid_branches_) {
    delete ptr;
  }
  for (auto &[name, ptr] : gp_key_branches_) {
    delete ptr;
  }

  // Clean up GP value branch pointers
  delete gp_int_branch_;
  delete gp_float_branch_;
  delete gp_double_branch_;
  delete gp_string_branch_;
}

DataSource::VertexPosition EDM4hepDataSource::getBeamVertexPosition() const {
  VertexPosition vertex{0.0f, 0.0f, 0.0f};

  // Check if we have particle data
  if (!mcparticle_branch_ || mcparticle_branch_->empty()) {
    return vertex;
  }

  // Get position of first particle with generatorStatus 1
  try {
    for (const auto &particle : *mcparticle_branch_) {
      if (particle.generatorStatus == 1) {
        vertex.x = particle.vertex.x;
        vertex.y = particle.vertex.y;
        vertex.z = particle.vertex.z;
        break;
      }
    }
  } catch (const std::exception &e) {
    std::cout << "Warning: Could not access MCParticles for beam vertex: "
              << e.what() << std::endl;
  }

  return vertex;
}

void EDM4hepDataSource::printStatus() const {
  std::cout << "=== EDM4hepDataSource Status ===" << std::endl;
  std::cout << "Source: " << source_index_ << " (" << config_->name << ")"
            << std::endl;
  std::cout << "Total entries: " << total_entries_ << std::endl;
  std::cout << "Current entry: " << current_entry_index_ << std::endl;
  std::cout << "Entries needed: " << entries_needed_ << std::endl;
  std::cout << "Initialized: " << (isInitialized() ? "Yes" : "No") << std::endl;

  if (tracker_collection_names_) {
    std::cout << "Tracker collections: " << tracker_collection_names_->size()
              << std::endl;
  }
  if (calo_collection_names_) {
    std::cout << "Calorimeter collections: " << calo_collection_names_->size()
              << std::endl;
  }
  std::cout << "================================" << std::endl;
}

std::string EDM4hepDataSource::getCorrespondingContributionCollection(
    const std::string &calo_collection_name) const {
  // Add "Contributions" suffix to get the contribution collection name
  return calo_collection_name + "Contributions";
}

std::string EDM4hepDataSource::getCorrespondingCaloCollection(
    const std::string &contrib_collection_name) const {
  // Remove "Contributions" suffix if present
  std::string base = contrib_collection_name;
  if (base.length() > 13 &&
      base.substr(base.length() - 13) == "Contributions") {
    base = base.substr(0, base.length() - 13);
  }
  return base;
}
