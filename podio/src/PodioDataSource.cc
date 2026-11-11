#include "PodioDataSource.h"
#include "PodioBufferOwnership.h"
#include "PodioBufferMerger.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>

PodioDataSource::PodioDataSource(const SourceConfig& config,
                                 size_t source_index) {
    config_ = &config;
    source_index_ = source_index;
    entries_needed_ = 1;
}

PodioDataSource::~PodioDataSource() {
    releasePodioCollectionBuffers(current_collection_buffers_);
}

void PodioDataSource::initialize(
    const std::vector<std::string>&,
    const std::vector<std::string>&,
    const std::vector<std::string>&) {
    if (config_->input_files.empty()) {
        throw std::runtime_error("Podio source " + config_->name +
                                 " has no input files");
    }
    reader_ = std::make_unique<podio::ROOTReader>();
    reader_->openFiles(config_->input_files);

    const auto categories = reader_->getAvailableCategories();
    const auto category = std::find(categories.begin(), categories.end(),
                                     config_->tree_name);
    if (category == categories.end()) {
        throw std::runtime_error("Podio category '" + config_->tree_name +
                                 "' is not available in source " +
                                 config_->name);
    }

    total_entries_ = reader_->getEntries(config_->tree_name);
    if (total_entries_ == 0) {
        throw std::runtime_error("Podio category '" + config_->tree_name +
                                 "' has no entries in source " +
                                 config_->name);
    }

    const auto skip = config_->skip > 0
                          ? static_cast<size_t>(config_->skip)
                          : size_t{0};
    if (skip >= total_entries_ &&
        !config_->repeat_on_eof) {
        throw std::runtime_error(
            "Skip value exceeds available entries and repeat_on_eof is false for source " +
            config_->name);
    }
    current_entry_index_ =
        config_->repeat_on_eof ? skip % total_entries_ : skip;
}

bool PodioDataSource::hasMoreEntries() const {
    if (config_->repeat_on_eof && total_entries_ > 0) {
        return true;
    }
    return current_entry_index_ + entries_needed_ <= total_entries_;
}

bool PodioDataSource::loadNextEvent() {
    if (!hasMoreEntries()) {
        return false;
    }

    loadEvent(current_entry_index_);
    ++current_entry_index_;
    return true;
}

void PodioDataSource::loadEvent(size_t event_index) {
    if (!reader_) {
        throw std::logic_error("Podio source must be initialized before reading");
    }
    if (current_frame_data_) {
        const auto requested_entry =
            total_entries_ == 0
                ? event_index
                : (config_->repeat_on_eof ? event_index % total_entries_
                                          : event_index);
        if (requested_entry == loaded_entry_index_) {
            return;
        }
        throw std::logic_error(
            "The previously loaded podio entry must be consumed before reading another");
    }
    if (total_entries_ == 0 || (event_index >= total_entries_ &&
                                !config_->repeat_on_eof)) {
        throw std::out_of_range("Podio entry index is outside the available entries");
    }

    const auto entry = config_->repeat_on_eof ? event_index % total_entries_
                                               : event_index;
    if (entry > std::numeric_limits<unsigned>::max()) {
        throw std::out_of_range("Podio entry index exceeds ROOTReader's range");
    }

    current_frame_data_ =
        reader_->readEntry(config_->tree_name, static_cast<unsigned>(entry));
    if (!current_frame_data_) {
        throw std::runtime_error("Failed to read podio entry " +
                                 std::to_string(entry) + " from source " +
                                 config_->name);
    }
    loaded_entry_index_ = entry;
    for (const auto& name : current_frame_data_->getAvailableCollections()) {
        auto buffers = current_frame_data_->getCollectionBuffers(name);
        if (!buffers) {
            throw std::runtime_error("Could not retrieve buffers for collection '" +
                                     name + "' in source '" + config_->name +
                                     "'");
        }
        current_collection_buffers_.emplace(name, std::move(*buffers));
    }
}

std::unique_ptr<podio::ROOTFrameData>
PodioDataSource::takeCurrentFrameData() {
    if (!current_frame_data_) {
        throw std::logic_error("No podio entry is loaded for source " +
                               config_->name);
    }
    return std::move(current_frame_data_);
}

std::unordered_map<std::string, podio::CollectionReadBuffers>
PodioDataSource::takeCurrentCollectionBuffers() {
    if (!current_frame_data_) {
        throw std::logic_error("No podio entry is loaded for source " +
                               config_->name);
    }
    auto buffers = std::move(current_collection_buffers_);
    current_collection_buffers_.clear();
    return buffers;
}

const std::unordered_map<std::string, podio::CollectionReadBuffers>&
PodioDataSource::getCurrentCollectionBuffers() const {
    if (!current_frame_data_) {
        throw std::logic_error("No podio entry is loaded for source " +
                               config_->name);
    }
    return current_collection_buffers_;
}

void PodioDataSource::printStatus() const {
    std::cout << "=== PodioDataSource Status ===\n"
              << "Source: " << source_index_ << " (" << config_->name << ")\n"
              << "Total entries: " << total_entries_ << "\n"
              << "Current entry: " << current_entry_index_ << "\n"
              << "Entries needed: " << entries_needed_ << "\n"
              << "Initialized: " << (isInitialized() ? "Yes" : "No")
              << std::endl;
}

bool PodioDataSource::isInitialized() const {
    return reader_ != nullptr;
}

DataSource::VertexPosition PodioDataSource::getBeamVertexPosition() const {
    const auto collection =
        current_collection_buffers_.find(config_->beam_vertex_collection);
    if (collection == current_collection_buffers_.end()) {
        throw std::runtime_error(
            "Beam-vertex collection '" + config_->beam_vertex_collection +
            "' is not present in source '" + config_->name + "'");
    }
    auto position =
        PodioBufferMerger::instance().beamVertex(collection->second);
    if (!position) {
        return {};
    }
    return {(*position)[0], (*position)[1], (*position)[2]};
}
