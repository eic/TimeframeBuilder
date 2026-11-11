#include "DataHandler.h"
#include "ParameterTransforms.h"
#ifdef HAVE_PODIO
#include "PodioDataHandler.h"
#endif
#ifdef HAVE_HEPMC3
#include "HepMC3DataHandler.h"
#endif
#include <iostream>
#include <stdexcept>

void DataHandler::mergeEvents(std::vector<std::unique_ptr<DataSource>>& sources,
                              size_t timeframe_number,
                              float timeframe_duration,
                              float bunch_crossing_period,
                              std::mt19937& gen) {
    current_timeframe_number_ = timeframe_number;
    
    size_t total_events_consumed = 0;
    // Iterate over all sources
    for (auto& source : sources) {
        const auto& config = source->getConfig();
        size_t entries_needed = source->getEntriesNeeded();
        size_t events_consumed = 0;
        
        // Process each event from this source
        for (size_t entry = 0; entry < entries_needed; ++entry) {
            // Load and prepare the event
            source->loadEvent(source->getCurrentEntryIndex());
            current_event_transform_context_ =
                EventTransformRegistry::instance().prepare(
                    *source, config, timeframe_duration,
                    bunch_crossing_period, gen, event_transform_rules_);
            
            // Call format-specific processing
            processEvent(*source);
            
            source->setCurrentEntryIndex(source->getCurrentEntryIndex() + 1);
            events_consumed++;
        }
        total_events_consumed += events_consumed;
        
        std::cout << "Processed " << events_consumed << " events from source " 
                  << config.name << std::endl;
    }

    std::cout << "Total events consumed in timeframe " << timeframe_number 
              << ": " << total_events_consumed << std::endl;
}

std::unique_ptr<DataHandler> DataHandler::create(const std::string& filename) {
    // Helper lambda to check file extension
    auto hasExtension = [](const std::string& filename, const std::string& ext) {
        if (filename.length() < ext.length()) return false;
        return filename.compare(filename.length() - ext.length(), ext.length(), ext) == 0;
    };
    
#ifdef HAVE_HEPMC3
    // Check if filename ends with .hepmc3.tree.root (more specific first)
    if (hasExtension(filename, ".hepmc3.tree.root")) {
        return std::make_unique<HepMC3DataHandler>();
    }
#endif

#ifdef HAVE_PODIO
    if (hasExtension(filename, ".podio.root") ||
        hasExtension(filename, ".edm4hep.root")) {
        return std::make_unique<PodioDataHandler>();
    }
#endif
    
    std::string error_msg = "Unsupported data format: " + filename + "\n"
        "Currently supported formats:\n";
#ifdef HAVE_PODIO
    error_msg +=
        "  - Files ending with '.podio.root' or '.edm4hep.root' (podio handler)\n";
#endif
#ifdef HAVE_HEPMC3
    error_msg += "  - Files ending with '.hepmc3.tree.root' (e.g., output.hepmc3.tree.root)";
#endif
#if !defined(HAVE_PODIO) && !defined(HAVE_HEPMC3)
    error_msg += "  - No format backend was enabled when this binary was built\n";
#endif
    throw std::runtime_error(error_msg);
}
