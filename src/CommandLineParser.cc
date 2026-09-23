#include "CommandLineParser.h"
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <getopt.h>
#include <glob.h>

void CommandLineParser::printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options] input_file1 [input_file2 ...]\n"
              << "\nGeneral Options:\n"
              << "  --config FILE                YAML config file\n"
              << "  -o, --output FILE           Output file name (default: merged_timeframes.edm4hep.root)\n"
              << "  -n, --nevents N             Maximum number of timeframes to generate (default: 100)\n"
              << "  -d, --duration TIME         Timeframe duration in ns (default: 20.0)\n"
              << "  -p, --bunch-period PERIOD   Bunch crossing period in ns (default: 10.0)\n"
              << "  --random-seed SEED          Random number generator seed (default: 0, use random_device)\n"
              << "  --reader BACKEND            Input backend: 'root' (TChain/TTree, default) or 'podio' (podio::ROOTFrameReader)\n"
              << "  --writer BACKEND            Output writer: 'root' (TTree, default) or 'podio' (podio::ROOTWriter frame)\n"
              << "  -h, --help                  Show this help message\n"
              << "  -v, --version               Show version information\n"
              << "\nDefault Source Options (backward compatibility):\n"
              << "  -f, --frequency FREQ        Mean event frequency (events/ns) (default: 1.0)\n"
              << "  -b, --use-bunch-crossing    Enable bunch crossing logic\n"
              << "  -s, --static-events         Use static number of events per timeframe\n"
              << "  -e, --events-per-frame N    Static events per timeframe (default: 1)\n"
              << "  --beam-attachment           Enable beam attachment with Gaussian smearing\n"
              << "  --beam-speed SPEED          Beam speed in mm/ns (default: 299.792458)\n"
              << "  --beam-spread SPREAD        Beam spread for Gaussian smearing (default: 0.0)\n"
              << "  --status-offset OFFSET      Generator status offset (default: 0)\n"
              << "\nSource-Specific Options:\n"
              << "  --source:NAME               Create or select source named NAME\n"
              << "  --source:NAME:input_files FILE1,FILE2\n"
              << "                              Input files for source (comma-separated)\n"
              << "  --source:NAME:skip N\n"
              << "                              Skip N events from source files\n"
              << "  --source:NAME:frequency FREQ\n"
              << "                              Mean event frequency for source\n"
              << "  --source:NAME:static_events BOOL\n"
              << "                              Use static events (true/false)\n"
              << "  --source:NAME:events_per_frame N\n"
              << "                              Static events per timeframe\n"
              << "  --source:NAME:bunch_crossing BOOL\n"
              << "                              Enable bunch crossing (true/false)\n"
              << "  --source:NAME:beam_attachment BOOL\n"
              << "                              Enable beam attachment (true/false)\n"
              << "  --source:NAME:beam_speed SPEED\n"
              << "                              Beam speed in mm/ns\n"
              << "  --source:NAME:beam_spread SPREAD\n"
              << "                              Beam spread for Gaussian smearing\n"
              << "  --source:NAME:status_offset OFFSET\n"
              << "                              Generator status offset\n"
              << "  --source:NAME:keep_weight BOOL\n"
              << "                              Keep event weights (true/false)\n"
              << "  --source:NAME:repeat_on_eof BOOL\n"
              << "                              Repeat source when EOF reached (true/false)\n"
              << "\nExamples:\n"
              << "  # Create signal source with specific files and frequency\n"
              << "  " << program_name << " --source:signal:input_files signal1.edm4hep.root,signal2.edm4hep.root --source:signal:frequency 0.5\n"
              << "  # Create background source with static events\n"
              << "  " << program_name << " --source:bg:input_files bg.edm4hep.root --source:bg:static_events true --source:bg:events_per_frame 2\n";
}

bool CommandLineParser::parseBool(const std::string& value) {
    std::string lower_value = value;
    std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(), ::tolower);
    return (lower_value == "true" || lower_value == "1" || lower_value == "yes" || lower_value == "on");
}

std::vector<std::string> CommandLineParser::splitCommaSeparated(const std::string& value) {
    std::vector<std::string> result;
    std::stringstream ss(value);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    return result;
}

std::vector<std::string> CommandLineParser::expandGlobPatterns(const std::vector<std::string>& patterns) {
    std::vector<std::string> expanded_files;
    for (const auto& pattern : patterns) {
        glob_t glob_result{};
        int ret = glob(pattern.c_str(), GLOB_TILDE, nullptr, &glob_result);

        if (ret == 0) {
            // matched files
            for (size_t i = 0; i < glob_result.gl_pathc; ++i) {
                expanded_files.emplace_back(glob_result.gl_pathv[i]);
            }
        } else if (ret == GLOB_NOMATCH) {
            bool has_wildcard = pattern.find_first_of("*?[") != std::string::npos;

            if (has_wildcard) {
                std::cerr << "Warning: No files matched wildcard pattern: " << pattern << std::endl;
            } else {
                // treat as literal filename (local or XRootD)
                expanded_files.emplace_back(pattern);
            }
        } else {
            std::cerr << "Error: glob() failed for pattern: " << pattern << std::endl;
        }

        globfree(&glob_result);
    }
    return expanded_files;
}

SourceConfig* CommandLineParser::findOrCreateSource(std::vector<SourceConfig>& sources, const std::string& name) {
    for (auto& source : sources) {
        if (source.name == name) {
            return &source;
        }
    }
    // Create new source
    SourceConfig new_source;
    new_source.name = name;
    sources.push_back(new_source);
    return &sources.back();
}

bool CommandLineParser::handleSourceOption(std::vector<SourceConfig>& sources, const std::string& option, const std::string& value) {
    // Expected format: source:name:property
    size_t first_colon = option.find(':');
    if (first_colon == std::string::npos || first_colon == 0) {
        return false;
    }
    
    std::string prefix = option.substr(0, first_colon);
    if (prefix != "source") {
        return false;
    }
    
    std::string rest = option.substr(first_colon + 1);
    size_t second_colon = rest.find(':');
    
    // Handle --source:name (create source)
    if (second_colon == std::string::npos) {
        std::string source_name = rest;
        findOrCreateSource(sources, source_name);
        return true;
    }
    
    // Handle --source:name:property
    std::string source_name = rest.substr(0, second_colon);
    std::string property = rest.substr(second_colon + 1);
    
    SourceConfig* source = findOrCreateSource(sources, source_name);
    
    if (property == "input_files") {
        std::vector<std::string> files = splitCommaSeparated(value);
        source->input_files  = expandGlobPatterns(files);
    } else if (property == "skip") {
        source->skip = std::stoi(value);
    } else if (property == "frequency") {
        source->mean_event_frequency = std::stof(value);
    } else if (property == "static_events") {
        source->static_number_of_events = parseBool(value);
    } else if (property == "events_per_frame") {
        source->static_events_per_timeframe = std::stoul(value);
    } else if (property == "bunch_crossing") {
        source->use_bunch_crossing = parseBool(value);
    } else if (property == "beam_attachment") {
        source->attach_to_beam = parseBool(value);
    } else if (property == "beam_speed") {
        source->beam_speed = std::stof(value);
    } else if (property == "beam_spread") {
        source->beam_spread = std::stof(value);
    } else if (property == "status_offset") {
        source->generator_status_offset = std::stoi(value);
    } else if (property == "keep_weight") {
        source->keep_weight = parseBool(value);
    } else if (property == "already_merged") {
        source->already_merged = parseBool(value);
    } else if (property == "tree_name") {
        source->tree_name = value;
    } else if (property == "beam_angle") {
        source->beam_angle = std::stof(value);
    } else if (property == "repeat_on_eof") {
        source->repeat_on_eof = parseBool(value);
    } else {
        std::cerr << "Warning: Unknown source property: " << property << std::endl;
        return false;
    }
    
    return true;
}

void CommandLineParser::loadYAMLConfig(const std::string& config_file, MergerConfig& config) {
    YAML::Node yaml = YAML::LoadFile(config_file);
    if (yaml["output_file"]) config.output_file = yaml["output_file"].as<std::string>();
    if (yaml["max_events"]) config.max_events = yaml["max_events"].as<size_t>();
    if (yaml["timeframe_duration"]) config.timeframe_duration = yaml["timeframe_duration"].as<float>();
    if (yaml["bunch_crossing_period"]) config.bunch_crossing_period = yaml["bunch_crossing_period"].as<float>();
    if (yaml["random_seed"]) config.random_seed = yaml["random_seed"].as<unsigned int>();
    if (yaml["introduce_offsets"]) config.introduce_offsets = yaml["introduce_offsets"].as<bool>();

    if (yaml["reader"]) config.reader = yaml["reader"].as<std::string>();
    if (yaml["writer"]) config.writer = yaml["writer"].as<std::string>();

    if (yaml["sources"]) {
        config.sources.clear();
        for (const auto& source_yaml : yaml["sources"]) {
            SourceConfig source;
            if (source_yaml["input_files"]) {
                for (const auto& f : source_yaml["input_files"]) {
                    std::vector<std::string> expanded_files = expandGlobPatterns({f.as<std::string>()});
                    source.input_files.insert(source.input_files.end(), expanded_files.begin(), expanded_files.end());
                }
            }
            if (source_yaml["name"]) source.name = source_yaml["name"].as<std::string>();
            if (source_yaml["skip"]) source.skip = source_yaml["skip"].as<long long>();
            if (source_yaml["already_merged"]) source.already_merged = source_yaml["already_merged"].as<bool>();
            if (source_yaml["static_number_of_events"]) source.static_number_of_events = source_yaml["static_number_of_events"].as<bool>();
            if (source_yaml["static_events_per_timeframe"]) source.static_events_per_timeframe = source_yaml["static_events_per_timeframe"].as<size_t>();
            if (source_yaml["mean_event_frequency"]) source.mean_event_frequency = source_yaml["mean_event_frequency"].as<float>();
            if (source_yaml["use_bunch_crossing"]) source.use_bunch_crossing = source_yaml["use_bunch_crossing"].as<bool>();
            if (source_yaml["attach_to_beam"]) source.attach_to_beam = source_yaml["attach_to_beam"].as<bool>();
            if (source_yaml["beam_angle"]) source.beam_angle = source_yaml["beam_angle"].as<float>();
            if (source_yaml["beam_speed"]) source.beam_speed = source_yaml["beam_speed"].as<float>();
            if (source_yaml["beam_spread"]) source.beam_spread = source_yaml["beam_spread"].as<float>();
            if (source_yaml["generator_status_offset"]) source.generator_status_offset = source_yaml["generator_status_offset"].as<int32_t>();
            if (source_yaml["keep_weight"]) source.keep_weight = source_yaml["keep_weight"].as<bool>();
            if (source_yaml["repeat_on_eof"]) source.repeat_on_eof = source_yaml["repeat_on_eof"].as<bool>();
            config.sources.push_back(source);
        }
    }
}

void CommandLineParser::mergeCliSources(MergerConfig& config, const std::vector<SourceConfig>& cli_sources) {
    // CLI sources can override YAML sources with the same name
    for (const auto& cli_source : cli_sources) {
        bool found = false;
        for (auto& existing_source : config.sources) {
            if (existing_source.name == cli_source.name) {
                // Override YAML source with CLI values (only non-default values)
                if (!cli_source.input_files.empty()) {
                    existing_source.input_files = cli_source.input_files;
                }
                if (cli_source.mean_event_frequency != 1.0f) {
                    existing_source.mean_event_frequency = cli_source.mean_event_frequency;
                }
                if (cli_source.static_number_of_events) {
                    existing_source.static_number_of_events = cli_source.static_number_of_events;
                }
                if (cli_source.static_events_per_timeframe != 1) {
                    existing_source.static_events_per_timeframe = cli_source.static_events_per_timeframe;
                }
                if (cli_source.use_bunch_crossing) {
                    existing_source.use_bunch_crossing = cli_source.use_bunch_crossing;
                }
                if (cli_source.attach_to_beam) {
                    existing_source.attach_to_beam = cli_source.attach_to_beam;
                }
                if (cli_source.beam_speed != 299.792458f) {
                    existing_source.beam_speed = cli_source.beam_speed;
                }
                if (cli_source.beam_spread != 0.0f) {
                    existing_source.beam_spread = cli_source.beam_spread;
                }
                if (cli_source.generator_status_offset != 0) {
                    existing_source.generator_status_offset = cli_source.generator_status_offset;
                }
                if (cli_source.keep_weight) {
                    existing_source.keep_weight = cli_source.keep_weight;
                }
                if (cli_source.skip != 0) {
                    existing_source.skip = cli_source.skip;
                }
                if (cli_source.already_merged) {
                    existing_source.already_merged = cli_source.already_merged;
                }
                if (cli_source.tree_name != "events") {
                    existing_source.tree_name = cli_source.tree_name;
                }
                if (cli_source.beam_angle != 0.0f) {
                    existing_source.beam_angle = cli_source.beam_angle;
                }
                if (cli_source.repeat_on_eof) {
                    existing_source.repeat_on_eof = cli_source.repeat_on_eof;
                }
                found = true;
                break;
            }
        }
        if (!found) {
            // Add new CLI source
            config.sources.push_back(cli_source);
        }
    }
}

void CommandLineParser::validateConfiguration(MergerConfig& config) {
    // Remove sources with no input files and warn about them
    auto it = config.sources.begin();
    while (it != config.sources.end()) {
        if (it->input_files.empty()) {
            std::cerr << "Warning: Source '" << it->name << "' has no input files specified - removing from configuration" << std::endl;
            it = config.sources.erase(it);
        } else {
            ++it;
        }
    }
    
    // After cleanup, ensure we have at least one valid source
    if (config.sources.empty()) {
        throw std::runtime_error("Error: No valid sources with input files specified");
    }
}

void CommandLineParser::printConfiguration(const MergerConfig& config) {
    std::cout << "=== Timeframe Merger Configuration ===" << std::endl;
    std::cout << "Sources: " << config.sources.size() << std::endl;
    for (size_t i = 0; i < config.sources.size(); ++i) {
        const auto& source = config.sources[i];        
        std::cout << "Source " << i << " input files: ";
        for (const auto& file : source.input_files) {
            std::cout << file << " ";
        }
        std::cout << std::endl;
        std::cout << "  Name: " << source.name << std::endl;
        std::cout << "  Events skipped: " << source.skip << std::endl;
        std::cout << "  Static number of events: " << (source.static_number_of_events ? "true" : "false") << std::endl;
        std::cout << "  Events per timeframe: " << source.static_events_per_timeframe << std::endl;
        std::cout << "  Mean event frequency: " << source.mean_event_frequency << " events/ns" << std::endl;
        std::cout << "  Use bunch crossing: " << (source.use_bunch_crossing ? "true" : "false") << std::endl;
        std::cout << "  Beam attachment: " << (source.attach_to_beam ? "true" : "false") << std::endl;
        std::cout << "  Beam speed: " << source.beam_speed << " mm/ns" << std::endl;
        std::cout << "  Beam spread: " << source.beam_spread << std::endl;
        std::cout << "  Generator status offset: " << source.generator_status_offset << std::endl;
        std::cout << "  Keep weight: " << (source.keep_weight ? "true" : "false") << std::endl;
        std::cout << "  Repeat on EOF: " << (source.repeat_on_eof ? "true" : "false") << std::endl;
    }
    std::cout << "Output file: " << config.output_file << std::endl;
    std::cout << "Max events: " << config.max_events << std::endl;
    std::cout << "Timeframe duration: " << config.timeframe_duration << " ns" << std::endl;
    std::cout << "Bunch crossing period: " << config.bunch_crossing_period << " ns" << std::endl;
    std::cout << "Random seed: " << config.random_seed << (config.random_seed == 0 ? " (using random_device)" : "") << std::endl;
    std::cout << "Introduce offsets: " << (config.introduce_offsets ? "true" : "false") << std::endl;
    std::cout << "Reader backend: " << config.reader << std::endl;
    std::cout << "Writer backend: " << config.writer << std::endl;
    std::cout << "================================================" << std::endl;
}

MergerConfig CommandLineParser::parse(int argc, char* argv[]) {
    MergerConfig config;
    SourceConfig default_source; // Default source config
    std::string config_file = "";
    std::vector<SourceConfig> cli_sources; // Sources defined via CLI
    
    // First pass: extract source-specific options and process them separately
    std::vector<char*> remaining_args;
    remaining_args.push_back(argv[0]); // program name
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg.find("--source:") == 0) {
            // Handle source-specific option
            std::string option_name = arg.substr(2); // Remove "--"
            std::string value = "";
            
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                value = argv[i + 1];
                i++; // Skip the value argument
            }
            
            if (!handleSourceOption(cli_sources, option_name, value)) {
                std::cerr << "Error: Invalid source option: " << arg << std::endl;
                throw std::runtime_error("Invalid source option");
            }
        } else {
            // Regular argument - keep for standard processing
            remaining_args.push_back(argv[i]);
        }
    }
    
    // Update argc/argv for standard getopt processing
    int new_argc = remaining_args.size();
    char** new_argv = remaining_args.data();
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 1004},
        {"output", required_argument, 0, 'o'},
        {"nevents", required_argument, 0, 'n'},
        {"duration", required_argument, 0, 'd'},
        {"frequency", required_argument, 0, 'f'},
        {"bunch-period", required_argument, 0, 'p'},
        {"random-seed", required_argument, 0, 1005},
        {"use-bunch-crossing", no_argument, 0, 'b'},
        {"static-events", no_argument, 0, 's'},
        {"events-per-frame", required_argument, 0, 'e'},
        {"beam-attachment", no_argument, 0, 1000},
        {"beam-speed", required_argument, 0, 1001},
        {"beam-spread", required_argument, 0, 1002},
        {"status-offset", required_argument, 0, 1003},
        {"keep-weight", required_argument, 0, 1006},
        {"reader", required_argument, 0, 1007},
        {"writer", required_argument, 0, 1008},
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(new_argc, new_argv, "o:n:d:f:p:bse:hv", long_options, &option_index)) != -1) {
        switch (opt) {
            case 1004:
                config_file = optarg;
                break;
            case 'o':
                config.output_file = optarg;
                break;
            case 'n':
                config.max_events = std::stoul(optarg);
                break;
            case 'd':
                config.timeframe_duration = std::stof(optarg);
                break;
            case 'f':
                default_source.mean_event_frequency = std::stof(optarg);
                break;
            case 'p':
                config.bunch_crossing_period = std::stof(optarg);
                break;
            case 'b':
                default_source.use_bunch_crossing = true;
                break;
            case 's':
                default_source.static_number_of_events = true;
                break;
            case 'e':
                default_source.static_events_per_timeframe = std::stoul(optarg);
                break;
            case 1000:
                default_source.attach_to_beam = true;
                break;
            case 1001:
                default_source.beam_speed = std::stof(optarg);
                break;
            case 1002:
                default_source.beam_spread = std::stof(optarg);
                break;
            case 1003:
                default_source.generator_status_offset = std::stoi(optarg);
                break;
            case 1006:
                default_source.keep_weight = parseBool(optarg);
                break;
            case 1005:
                config.random_seed = std::stoul(optarg);
                break;
            case 1007:
                config.reader = optarg;
                if (config.reader != "root" && config.reader != "podio") {
                    throw std::runtime_error("Invalid --reader value: '" + config.reader + "'. Must be 'root' or 'podio'.");
                }
                break;
            case 1008:
                config.writer = optarg;
                if (config.writer != "root" && config.writer != "podio") {
                    throw std::runtime_error("Invalid --writer value: '" + config.writer + "'. Must be 'root' or 'podio'.");
                }
                break;
            case 'h':
                printUsage(new_argv[0]);
                std::exit(0);
            case 'v':
                std::cout << "TimeframeBuilder version: " << TIMEFRAME_BUILDER_VERSION_STR << std::endl;
                std::exit(0);
            default:
                printUsage(new_argv[0]);
                throw std::runtime_error("Invalid command-line arguments");
        }
    }
    
    // If config_file specified, parse YAML
    if (!config_file.empty()) {
        loadYAMLConfig(config_file, config);
    }
    
    // Merge CLI sources with existing config sources
    mergeCliSources(config, cli_sources);
    
    // Command-line input files override YAML - add to default source
    for (int i = optind; i < new_argc; i++) {
        default_source.input_files.push_back(new_argv[i]);
    }
    
    // If we have command-line files or no sources from any method, add default source
    if (!default_source.input_files.empty() || (config.sources.empty() && cli_sources.empty())) {
        config.sources.push_back(default_source);
    }
    
    // Validate configuration
    validateConfiguration(config);
    
    // Print configuration
    printConfiguration(config);
    
    return config;
}
