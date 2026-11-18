// DataSource.cc - Base class implementation for shared functionality
#include "DataSource.h"
#include <cmath>

void DataSource::UpdateTimeOffset(float timeframe_duration,
                                  float bunch_crossing_period,
                                  float global_phase,
				  int   start_crossing,
				  size_t crossings_per_rev,
				  const std::vector<float>& populated_crossing_times,
                                  std::mt19937& rng) {
    float distance = 0.0f;
    
    // Calculate beam distance if needed (format-specific)
    if (!getConfig().already_merged && getConfig().attach_to_beam) {
        distance = calculateBeamDistance();
    }
    
    current_time_offset_ = generateTimeOffset(distance, timeframe_duration, bunch_crossing_period, global_phase,
                                              start_crossing, crossings_per_rev, populated_crossing_times, rng);
}

float DataSource::generateTimeOffset(float distance, float timeframe_duration, 
                                     float bunch_crossing_period, float global_phase, int start_crossing, \
				     size_t crossings_per_rev, const std::vector<float>& populated_crossing_times, std::mt19937& rng) const {
    const auto& config = getConfig();
    
    std::uniform_int_distribution<int> uniform_int(0, populated_crossing_times.size());
    float time_offset = populated_crossing_times[uniform_int(rng)];
    
    if (!config.already_merged) {
        
        // Apply bunch crossing if enabled
        if (config.use_bunch_crossing) {
          time_offset += (global_phase * bunch_crossing_period);
        }
        
        // Apply beam effects if enabled
        if (config.attach_to_beam) {
	  
            // randomize within the bunch_crossing_period
            std::uniform_real_distribution<float> uniform(0, bunch_crossing_period);
            time_offset += uniform(rng); 

            // Add time offset based on distance along beam
	    time_offset += distance / config.beam_speed + (global_phase * bunch_crossing_period);
            
            // Add Gaussian spread if specified
            if (config.beam_spread > 0.0f) {
                std::normal_distribution<float> spread_dist(0.0f, config.beam_spread);
                time_offset += spread_dist(rng);
            }
        }
    }
    else{

      // randomize within the bunch_crossing_period
      std::uniform_real_distribution<float> uniform(0, bunch_crossing_period);
      time_offset += uniform(rng); 
      
    }
    
    return time_offset;
}

float DataSource::calculateBeamDistance() const {
    const auto& config = getConfig();
    auto vertex = getBeamVertexPosition();
    
    // Distance is dot product of position vector relative to rotation around y of beam relative to z-axis
    return vertex.z * std::cos(config.beam_angle) + vertex.x * std::sin(config.beam_angle);
}
