#include "DataHandler.h"
#include "EDM4hepDataHandler.h"
#ifdef HAVE_HEPMC3
#include "HepMC3DataHandler.h"
#endif
#include <cmath>
#include <stdexcept>

void DataHandler::mergeEvents(std::vector<std::unique_ptr<DataSource>> &sources,
                              size_t timeframe_number, float timeframe_duration,
                              float bunch_crossing_period, float global_phase,
                              size_t crossings_per_rev,
                              const std::vector<bool> &bunch_pattern,
                              std::mt19937 &gen) {
  current_timeframe_number_ = timeframe_number;

  std::uniform_int_distribution<size_t> uniform(0, crossings_per_rev - 1);
  size_t crossing = uniform(gen);
  const size_t crossings_in_timeframe =
      static_cast<size_t>(std::floor(timeframe_duration / bunch_crossing_period));
  std::vector<float> populated_crossing_times;
  for (size_t crossing_in_timeframe = 0;
       crossing_in_timeframe < crossings_in_timeframe;
       ++crossing_in_timeframe) {
    if (bunch_pattern[crossing]) {
      populated_crossing_times.push_back(crossing_in_timeframe *
                                          bunch_crossing_period);
    }
    crossing = (crossing + 1) % crossings_per_rev;
  }
  const double live_fraction =
      static_cast<double>(populated_crossing_times.size()) /
      crossings_in_timeframe;

  size_t total_events_consumed = 0;
  // Iterate over all sources
  for (auto &source : sources) {
    const auto &config = source->getConfig();
    size_t entries_needed = source->getEntriesNeeded();
    if (!config.already_merged && !config.static_number_of_events &&
        (config.attach_to_beam || config.use_bunch_crossing)) {
      entries_needed = std::poisson_distribution<>(
          timeframe_duration * live_fraction * config.mean_event_frequency)(gen);
    }
    int events_consumed = 0;

    // Process each event from this source
    for (size_t entry = 0; entry < entries_needed; ++entry) {
      // Load and prepare the event
      source->loadEvent(source->getCurrentEntryIndex());
      source->UpdateTimeOffset(timeframe_duration, bunch_crossing_period,
                               global_phase, populated_crossing_times, gen);

      // Call format-specific processing
      processEvent(*source);

      source->setCurrentEntryIndex(source->getCurrentEntryIndex() + 1);
      events_consumed++;
    }
    total_events_consumed += events_consumed;

    std::cout << "Processed " << events_consumed << " events from source "
              << config.name << std::endl;
  }

  std::cout << "Total events consumed in timeframe " << timeframe_number << ": "
            << total_events_consumed << std::endl;
}

std::unique_ptr<DataHandler> DataHandler::create(const std::string &filename) {
  // Helper lambda to check file extension
  auto hasExtension = [](const std::string &filename, const std::string &ext) {
    if (filename.length() < ext.length())
      return false;
    return filename.compare(filename.length() - ext.length(), ext.length(),
                            ext) == 0;
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
  std::string error_msg =
      "Unsupported data format: " + filename +
      "\n"
      "Currently supported formats:\n"
      "  - Files ending with '.edm4hep.root' (e.g., output.edm4hep.root)\n";
#ifdef HAVE_HEPMC3
  error_msg += "  - Files ending with '.hepmc3.tree.root' (e.g., "
               "output.hepmc3.tree.root)";
#else
  error_msg +=
      "\nHepMC3 support not available (HepMC3 library not found during build)";
#endif
  throw std::runtime_error(error_msg);
}
