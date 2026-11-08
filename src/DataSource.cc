// DataSource.cc - Base class implementation for shared functionality
#include "DataSource.h"
#include <cmath>

void DataSource::UpdateTimeOffset(float timeframe_duration,
                                  float bunch_crossing_period,
				  float global_phase,
                                  std::mt19937& rng) {
    float distance = 0.0f;
    
    // Calculate beam distance if needed (format-specific)
    if (!getConfig().already_merged && getConfig().attach_to_beam) {
        distance = calculateBeamDistance();
    }
    
    current_time_offset_ = generateTimeOffset(distance, timeframe_duration, bunch_crossing_period, global_phase, rng);
}

float DataSource::generateTimeOffset(float distance, float timeframe_duration, 
                                     float bunch_crossing_period, float global_phase, std::mt19937& rng) const {
    const auto& config = getConfig();
    
    std::uniform_real_distribution<float> uniform(0.0f, timeframe_duration);
    float time_offset = uniform(rng);
    
    if (!config.already_merged) {
        // Apply bunch crossing if enabled
        if (config.use_bunch_crossing) {
            time_offset = std::floor(time_offset / bunch_crossing_period) * bunch_crossing_period + 0.5*bunch_crossing_period + global_phase;
        }
        
        // Apply beam effects if enabled
        if (config.attach_to_beam) {
            // Add time offset based on distance along beam
            time_offset += distance / config.beam_speed + global_phase;
            
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
