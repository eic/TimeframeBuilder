#pragma once

#include <string>
#include <unordered_map>
#include <vector>

struct EventTransformRule {
    std::string plugin;
    std::vector<std::string> sources{"all"};
    std::unordered_map<std::string, std::string> parameters;
};

struct RecordTransformRule {
    std::string plugin;
    std::string datatype{"all"};
    std::string collection{"all"};
    std::string field;
    std::unordered_map<std::string, std::string> parameters;
};

struct TransformationConfig {
    std::vector<EventTransformRule> event_transformations;
    std::vector<RecordTransformRule> record_transformations;
};


struct SourceConfig {
    // Input/output configuration
    std::vector<std::string> input_files;
    std::string name{"signal"};

    bool   already_merged{false};

    long long  skip{0};

    bool   static_number_of_events{false};
    size_t static_events_per_timeframe{1};
    float  mean_event_frequency{1.0f};
    bool   use_bunch_crossing{false};

    // Beam background config, beam direction and speed
    bool   attach_to_beam{false};
    std::string beam_vertex_collection{"MCParticles"};
    float  beam_angle{0.0f};
    float  beam_speed{299.792458f}; //Speed of light in mm/ns
    float  beam_spread{0.0f};

    // New generator status offset
    int32_t  generator_status_offset{0};
    bool     keep_weight{false};

    // Tree properties
    std::string tree_name{"events"};
    bool repeat_on_eof{false};
};

struct MergerConfig {
    float  timeframe_duration{2000.0f};
    float  bunch_crossing_period{10.0f};
    unsigned int random_seed{0};  // 0 means use random_device

    // Config per source
    std::vector<SourceConfig> sources;

    // Input/output configuration
    std::string output_file{"merged_timeframes.edm4hep.root"};
    size_t max_events{100};
};

struct TimeframeBuilderConfig {
    MergerConfig merger;
    TransformationConfig transformations;
};