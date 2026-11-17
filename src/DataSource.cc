// DataSource.cc - Base class implementation for shared functionality
#include "DataSource.h"
#include <cmath>

void DataSource::UpdateTimeOffset(float timeframe_duration,
                                  float bunch_crossing_period,
                                  float global_phase,
				  int   start_crossing,
				  size_t crossings_per_rev,
				  const std::vector<bool>& bunch_pattern,
                                  std::mt19937& rng) {
    float distance = 0.0f;
    
    // Calculate beam distance if needed (format-specific)
    if (!getConfig().already_merged && getConfig().attach_to_beam) {
        distance = calculateBeamDistance();
    }
    
    current_time_offset_ = generateTimeOffset(distance, timeframe_duration, bunch_crossing_period, global_phase,
                                              start_crossing, crossings_per_rev, bunch_pattern, rng);
}

float DataSource::generateTimeOffset(float distance, float timeframe_duration, 
                                     float bunch_crossing_period, float global_phase, int start_crossing, \
				     size_t crossings_per_rev, const std::vector<bool>& bunch_pattern, std::mt19937& rng) const {
    const auto& config = getConfig();
    
    std::uniform_real_distribution<float> uniform(0.0f, timeframe_duration);
    float time_offset = uniform(rng);
    
    if (!config.already_merged) {
        
	// Constrain to a valid bunch crossing
        if(config.use_bunch_crossing || config.attach_to_beam){
          bool found = false; 
	  do{
	    int cross_in_timeframe = std::floor(time_offset / bunch_crossing_period);
	    std::div_t cross_in_pattern = std::div(std::div(cross_in_timeframe, crossings_per_rev).rem + start_crossing, crossings_per_rev);
	    if(!bunch_pattern[cross_in_pattern.rem])
	      time_offset = uniform(rng);
	    else
	      found = true;
	  }while(!found);
        }

        // Apply bunch crossing if enabled
        if (config.use_bunch_crossing) {	    
          time_offset = std::floor(time_offset / bunch_crossing_period) * bunch_crossing_period + global_phase * bunch_crossing_period;	    
        }
        
        // Apply beam effects if enabled
        if (config.attach_to_beam) {
            // Add time offset based on distance along beam
            time_offset += distance / config.beam_speed + global_phase * bunch_crossing_period;
            
            // Add Gaussian spread if specified
            if (config.beam_spread > 0.0f) {
                std::normal_distribution<float> spread_dist(0.0f, config.beam_spread);
                time_offset += spread_dist(rng);
            }
        }
    }
    
    return time_offset;
}

float DataSource::calculateBeamDistance() const {
    const auto& config = getConfig();
    auto vertex = getBeamVertexPosition();
    
    // Distance is dot product of position vector relative to rotation around y of beam relative to z-axis
    return vertex.z * std::cos(config.beam_angle) + vertex.x * std::sin(config.beam_angle);
}
