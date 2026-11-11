#include "EventMerger.h"

#include <iostream>
#include <stdexcept>
#include <chrono>
#include <utility>

EventMerger::EventMerger(MergerConfig config,
                         TransformationConfig transformations,
                         std::unique_ptr<DataHandler> data_handler)
    : m_config(std::move(config)),
      transformations_(std::move(transformations)),
      gen(m_config.random_seed == 0 ? rd() : m_config.random_seed),
      data_handler_(std::move(data_handler)) {
    if (!data_handler_) {
        throw std::invalid_argument("EventMerger requires a data handler");
    }
}

void EventMerger::run() {
    std::cout << "Starting timeframe builder..." << std::endl;
    std::cout << "Sources: " << m_config.sources.size() << std::endl;
    std::cout << "Output file: " << m_config.output_file << std::endl;
    std::cout << "Max events: " << m_config.max_events << std::endl;
    std::cout << "Timeframe duration: " << m_config.timeframe_duration << std::endl;

    // Initialize data sources via the data handler
    // The data handler creates appropriate data sources for its format
    data_sources_ = data_handler_->initializeDataSources(m_config.output_file, m_config.sources);

    data_handler_->initializeOutput(m_config, transformations_, data_sources_);

    std::cout << "Processing " << m_config.max_events << " timeframes..." << std::endl;

    // Timing start
    auto start_time = std::chrono::high_resolution_clock::now();
    size_t events_generated = 0;
    for (; events_generated < m_config.max_events; ++events_generated) {
        // Update number of events needed per source
        if (!updateInputNEvents(data_sources_)) {
            std::cout << "Reached end of input data, stopping at " << events_generated
                      << " timeframes" << std::endl;
            break;
        }

        // Prepare for new timeframe
        data_handler_->prepareTimeframe();

        // Merge events from all sources
        data_handler_->mergeEvents(
            data_sources_, events_generated, m_config.timeframe_duration,
            m_config.bunch_crossing_period, gen);

        // Write the timeframe
        data_handler_->writeTimeframe();

        if (events_generated % 10 == 0) {
            std::cout << "Processed " << events_generated << " timeframes..." << std::endl;
        }
    }
    // Timing end
    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end_time - start_time;
    double total_time = elapsed.count();
    double avg_time_per_event = (events_generated > 0) ? total_time / events_generated : 0.0;
    std::cout << "\nTiming report:" << std::endl;
    std::cout << "  Total time: " << total_time << " s" << std::endl;
    std::cout << "  Number of events: " << events_generated << std::endl;
    std::cout << "  Average time per event: " << avg_time_per_event << " s" << std::endl;

    // Finalize output
    data_handler_->finalize();

    std::cout << "Merging complete. Total timeframes processed: " << events_generated << std::endl;
    std::cout << "Output saved to: " << m_config.output_file << std::endl;
}

bool EventMerger::updateInputNEvents(std::vector<std::unique_ptr<DataSource>>& sources) {
    for (auto& data_source : sources) {
        const auto& config = data_source->getConfig();

        // Generate new number of events needed for this source
        if (config.already_merged) {
            // Already merged sources should only contribute 1 event (which is already a full timeframe)
            data_source->setEntriesNeeded(1);
        } else if (config.static_number_of_events) {
            data_source->setEntriesNeeded(config.static_events_per_timeframe);
        } else {
            // Use Poisson for this source
            float mean_freq = config.mean_event_frequency;
            std::poisson_distribution<> poisson_dist(m_config.timeframe_duration * mean_freq);
            size_t n = poisson_dist(gen);
            data_source->setEntriesNeeded(n);
        }

        // Check enough events are available in this source
        if (!data_source->hasMoreEntries() && !config.repeat_on_eof) {
            std::cout << "Not enough events available in source " << config.name << std::endl;
            return false;
        }
    }

    return true;
}