#include "HepMC3DataSource.h"
#include <iostream>
#include <stdexcept>

HepMC3DataSource::HepMC3DataSource(const SourceConfig& config, size_t source_index) {
    config_ = &config;
    source_index_ = source_index;
    total_entries_ = 0;
    current_entry_index_ = 0;
    entries_needed_ = 0;
    m_still_to_skip = config_->skip;
    m_current_file = 0;
    m_total_files = config_->input_files.size();
    openNextFile();
}

HepMC3DataSource::~HepMC3DataSource() {
    cleanup();
}

void HepMC3DataSource::openNextFile() {
    if (m_current_file >= config_->input_files.size()) {
        if (!config_->repeat_on_eof) {
            throw std::runtime_error("No more input files to process for source: " + config_->name);
        } else {
            std::cout << "Reached end of input files for source: " << config_->name 
                      << ". Repeating from the first file." << std::endl;
            m_current_file = 0; // Loop back to the first file if repeat_on_eof is true
        }
    }

    const std::string& input_file = config_->input_files[m_current_file];

    // Validate file extension
    if (input_file.find(".hepmc3.tree.root") == std::string::npos) {
        throw std::runtime_error(
            "HepMC3DataSource only supports .hepmc3.tree.root format. Got: " + input_file
        );
    }

    std::cout << "Opening HepMC3 file: " << input_file << std::endl;

    // Create ReaderRootTree explicitly for ROOT tree format
    reader_ = std::make_shared<HepMC3::ReaderRootTree>(input_file);
    if (!reader_) {
        throw std::runtime_error("Failed to open HepMC3 file: " + input_file);
    }

    // Get entry count directly from TTree
    total_entries_ = reader_->m_tree->GetEntries();

    std::cout << "Found " << total_entries_ << " events in HepMC3 file" << std::endl;

    current_entry_index_ = 0;

    // Skip initial events if specified
    if (m_still_to_skip > 0) {
        std::cout << "Skipping first " << m_still_to_skip << " events for source " << config_->name << " as per configuration" << std::endl;
        if (m_still_to_skip >= total_entries_) {
            std::cout << "Warning: Skip value exceeds total entries, moving to next file." << std::endl;
            m_still_to_skip = m_still_to_skip - total_entries_; // Wrap around if repeat_on_eof is true
            m_current_file++;
            openNextFile(); // Open next file if available
            return;
        } else {
            reader_->skip(m_still_to_skip);
            current_entry_index_ += m_still_to_skip;
            m_still_to_skip = 0; // Reset skip counter after skipping
        }
    }
}

bool HepMC3DataSource::hasMoreEntries() const {
    return current_entry_index_ + entries_needed_ <= total_entries_;
}

bool HepMC3DataSource::loadNextEvent() {
    if (current_entry_index_ >= total_entries_) {
        openNextFile(); // Open next file if available
    }
    
    if (reader_->failed()) {
        return false;
    }
    
    // Read the next event
    if (!reader_->read_event(current_event_)) {
        return false;
    }
    
    return true;
}

void HepMC3DataSource::loadEvent(size_t event_index) {
    // HepMC3 readers are sequential, we can't jump to arbitrary positions
    // This would require reopening and skipping events
    // For now, just ensure we're reading sequentially
    if (event_index != current_entry_index_) {
        std::cerr << "Warning: HepMC3DataSource only supports sequential reading. "
                  << "Requested event " << event_index << " but at " << current_entry_index_ << std::endl;
    }
    loadNextEvent();
}

void HepMC3DataSource::cleanup() {
    if (reader_) {
        reader_->close();
        reader_.reset();
    }
}

void HepMC3DataSource::printStatus() const {
    std::cout << "HepMC3DataSource Status:" << std::endl;
    std::cout << "  Name: " << config_->name << std::endl;
    std::cout << "  Source Index: " << source_index_ << std::endl;
    std::cout << "  Total Entries: " << total_entries_ << std::endl;
    std::cout << "  Current Entry: " << current_entry_index_ << std::endl;
    std::cout << "  Entries Needed: " << entries_needed_ << std::endl;
    std::cout << "  Current Time Offset: " << current_time_offset_ << std::endl;
}

DataSource::VertexPosition HepMC3DataSource::getBeamVertexPosition() const {
    VertexPosition pos{0.0f, 0.0f, 0.0f};
    
    // Get the first vertex in the event (production vertex)
    const auto& vertices = current_event_.vertices();
    if (!vertices.empty() && vertices[0]) {
        const auto& vertex_pos = vertices[0]->position();
        pos.x = static_cast<float>(vertex_pos.x());
        pos.y = static_cast<float>(vertex_pos.y());
        pos.z = static_cast<float>(vertex_pos.z());
    }
    
    return pos;
}
