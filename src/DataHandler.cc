#include "DataHandler.h"
#include "EDM4hepDataHandler.h"
#include "PodioEDM4hepDataHandler.h"
#ifdef HAVE_ARROW
#include "ArrowInputDataHandler.h"
#endif
#ifdef HAVE_HEPMC3
#include "HepMC3DataHandler.h"
#endif
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
        int events_consumed = 0;
        
        // Process each event from this source
        for (size_t entry = 0; entry < entries_needed; ++entry) {
            // Load and prepare the event
            source->loadEvent(source->getCurrentEntryIndex());
            source->UpdateTimeOffset(timeframe_duration, bunch_crossing_period, gen);
            
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

std::unique_ptr<DataHandler> DataHandler::create(const MergerConfig& config) {
    const std::string& filename = config.output_file;
    const std::string& reader   = config.reader;

    // Helper lambda to check file extension
    auto hasExtension = [](const std::string& fn, const std::string& ext) {
        if (fn.length() < ext.length()) return false;
        return fn.compare(fn.length() - ext.length(), ext.length(), ext) == 0;
    };

#ifdef HAVE_HEPMC3
    // Check if filename ends with .hepmc3.tree.root (more specific first)
    if (hasExtension(filename, ".hepmc3.tree.root")) {
        return std::make_unique<HepMC3DataHandler>();
    }
#endif

    // Check if filename ends with .edm4hep.root
    if (hasExtension(filename, ".edm4hep.root")) {
        if (reader == "podio") {
            return std::make_unique<PodioEDM4hepDataHandler>();
        }
#ifdef HAVE_ARROW
        if (reader == "arrow") {
            // Arrow reader + ROOT TTree output (default)
            return std::make_unique<ArrowInputDataHandler>();
        }
#endif
        return std::make_unique<EDM4hepDataHandler>();
    }

    // Unsupported format
    std::string error_msg = "Unsupported data format: " + filename + "\n"
        "Currently supported formats:\n"
        "  - Files ending with '.edm4hep.root' (e.g., output.edm4hep.root)\n";
#ifdef HAVE_HEPMC3
    error_msg += "  - Files ending with '.hepmc3.tree.root' (e.g., output.hepmc3.tree.root)";
#else
    error_msg += "\nHepMC3 support not available (HepMC3 library not found during build)";
#endif
    throw std::runtime_error(error_msg);
}
