#pragma once

#include <string>
#include <vector>


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
    bool   introduce_offsets{true};
    float  timeframe_duration{2000.0f};
    float  bunch_crossing_period{10.0f};
    unsigned int random_seed{0};  // 0 means use random_device

    // Config per source
    std::vector<SourceConfig> sources;

    // Input/output configuration
    std::string output_file{"merged_timeframes.edm4hep.root"};
    size_t max_events{100};
    bool   merge_particles{false};

    // Backend reader: "root" (TChain/TTree, default) or "podio" (podio::ROOTFrameReader)
    std::string reader{"root"};
};