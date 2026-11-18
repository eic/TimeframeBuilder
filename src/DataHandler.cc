#include "DataHandler.h"
#include "EDM4hepDataHandler.h"
#ifdef HAVE_HEPMC3
#include "HepMC3DataHandler.h"
#endif
#include <stdexcept>

void DataHandler::mergeEvents(std::vector<std::unique_ptr<DataSource>>& sources,
                              size_t timeframe_number,
                              float timeframe_duration,
                              float bunch_crossing_period,
                              float global_phase,
			      size_t crossings_per_rev,
			      const std::vector<bool>& bunch_pattern,
                              std::mt19937& gen) {
    current_timeframe_number_ = timeframe_number;

    // randomly select the starting crossing position for the timeframe
    std::uniform_int_distribution<size_t> uniform(0,(int)crossings_per_rev-1);
    int start_crossing = uniform(gen);

    // Calculate the populated vs. empty crossings in the timeframe
    size_t populated_cross = 0;
    size_t empty_cross = 0;
    int cnt = start_crossing;
    float cnt_time = 0.0; 
    do{
      if(bunch_pattern[cnt])
        populated_cross++;
      else
        empty_cross++;
      cnt++;
      if(cnt>=crossings_per_rev) cnt = 0; 
      cnt_time += bunch_crossing_period;
    }while(cnt_time<timeframe_duration); 
    
    size_t total_events_consumed = 0;
    // Iterate over all sources
    for (auto& source : sources) {
        const auto& config = source->getConfig();
        size_t entries_needed = source->getEntriesNeeded();
	// scale entries if not forced or timeframe has no valid crossings 
        if((!config.static_number_of_events) || (populated_cross==0)) entries_needed = static_cast<size_t>(entries_needed*(static_cast<float>(populated_cross)/(populated_cross + empty_cross)));

        int events_consumed = 0;
        
        // Process each event from this source
        for (size_t entry = 0; entry < entries_needed; ++entry) {
            // Load and prepare the event
            source->loadEvent(source->getCurrentEntryIndex());
            source->UpdateTimeOffset(timeframe_duration, bunch_crossing_period, global_phase,
                                     start_crossing, crossings_per_rev, bunch_pattern, gen);
            
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
    
    // Check if filename ends with .edm4hep.root
    if (hasExtension(filename, ".edm4hep.root")) {
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
