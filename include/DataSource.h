#pragma once

#include "MergerConfig.h"
#include <memory>
#include <vector>
#include <string>
#include <random>

/**
 * @class DataSource
 * @brief Abstract base class for input data sources with pluggable format support
 * 
 * This abstract class defines the interface for reading event data from various
 * input formats (EDM4hep, HepMC3, etc.). Concrete implementations handle
 * format-specific logic while the merger remains format-agnostic.
 */
class DataSource {
public:
    virtual ~DataSource() = default;
    
    // Initialization - optional for formats that don't need collection names
    virtual void initialize(const std::vector<std::string>& tracker_collections,
                           const std::vector<std::string>& calo_collections,
                           const std::vector<std::string>& gp_collections) {
        // Default implementation does nothing - override if needed
    }
    
    // Data access
    virtual bool hasMoreEntries() const = 0;
    size_t getTotalEntries() const { return total_entries_; }
    size_t getCurrentEntryIndex() const { return current_entry_index_; }
    void setCurrentEntryIndex(size_t index) { current_entry_index_ = index; }
    float getCurrentTimeOffset() const { return current_time_offset_; }

    // Event management
    void setEntriesNeeded(size_t entries) { entries_needed_ = entries; }
    size_t getEntriesNeeded() const { return entries_needed_; }
    virtual bool loadNextEvent() = 0;
    
    // Event loading and time offset update
    virtual void loadEvent(size_t event_index) = 0;
    void UpdateTimeOffset(float timeframe_duration, float bunch_crossing_period,
                         float global_phase, std::mt19937& rng);
    
    // Configuration access
    const SourceConfig& getConfig() const { return *config_; }
    const std::string& getName() const { return config_->name; }
    size_t getSourceIndex() const { return source_index_; }
    
    // Status and diagnostics
    virtual void printStatus() const = 0;
    virtual bool isInitialized() const = 0;
    
    // Format-specific getters (to be implemented by concrete classes)
    virtual std::string getFormatName() const = 0;

protected:
    // Configuration (shared across implementations)
    const SourceConfig* config_ = nullptr;
    size_t source_index_ = 0;
    
    // State variables (shared across implementations)
    size_t total_entries_ = 0;
    size_t current_entry_index_ = 0;
    size_t entries_needed_ = 0;
    
    // Time offset state (shared across implementations)
    float current_time_offset_ = 0.0f;
    
    // Structure to hold vertex position
    struct VertexPosition {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };
    
    // Format-specific vertex extraction (must be implemented by derived classes)
    virtual VertexPosition getBeamVertexPosition() const = 0;
    
    // Shared beam distance calculation using vertex and beam angle
    float calculateBeamDistance() const;
    
    // Shared time offset generation logic
    float generateTimeOffset(float distance, float timeframe_duration, 
                            float bunch_crossing_period, float global_phase, std::mt19937& rng) const;
};
