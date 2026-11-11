#pragma once

#include "MergerConfig.h"
#include <string>
#include <vector>

/**
 * @class CommandLineParser
 * @brief Handles configuration and parameter parsing from command-line arguments and YAML files
 */
class CommandLineParser {
public:
    /**
     * Parse command-line arguments and optional YAML configuration file
     * @param argc Number of command-line arguments
     * @param argv Command-line arguments array
     * @return Populated run and transformation configurations
     * @throws std::runtime_error if required parameters are missing
     */
    static TimeframeBuilderConfig parse(int argc, char* argv[]);

private:
    /**
     * Print usage information to console
     * @param program_name Name of the program
     */
    static void printUsage(const char* program_name);

    /**
     * Parse boolean values from strings (handles "true", "1", "yes", "on")
     * @param value String representation of boolean
     * @return Boolean value
     */
    static bool parseBool(const std::string& value);

    /**
     * Split comma-separated values into a vector
     * @param value Comma-separated string
     * @return Vector of individual values
     */
    static std::vector<std::string> splitCommaSeparated(const std::string& value);

    /**
     * Expand glob patterns in file paths (e.g., "*.root") to actual file lists
     * @param patterns Vector of glob patterns
     * @return Vector of expanded file paths
     */
    static std::vector<std::string> expandGlobPatterns(const std::vector<std::string>& patterns);

    /**
     * Find or create a source configuration by name
     * @param sources Vector of source configurations
     * @param name Name of the source
     * @return Pointer to the source configuration
     */
    static SourceConfig* findOrCreateSource(std::vector<SourceConfig>& sources, const std::string& name);

    /**
     * Handle source-specific command-line options
     * @param sources Vector of source configurations
     * @param option Option string (format: source:name:property)
     * @param value Option value
     * @return True if option was handled successfully
     */
    static bool handleSourceOption(std::vector<SourceConfig>& sources, const std::string& option, const std::string& value);

    /**
     * Load configuration from YAML file
     * @param config_file Path to YAML configuration file
     * @param config MergerConfig to populate
     */
    static void loadYAMLConfig(const std::string& config_file, MergerConfig& config);

    /**
     * Load application-level plugin bindings from a transformation profile
     */
    static void loadTransformationYAMLConfig(
        const std::string& config_file, TransformationConfig& config);

    /**
     * Merge CLI-specified sources with YAML-loaded sources
     * CLI sources can override YAML sources with the same name
     * @param config MergerConfig to merge into
     * @param cli_sources Sources specified on command line
     */
    static void mergeCliSources(MergerConfig& config, const std::vector<SourceConfig>& cli_sources);

    /**
     * Validate the final configuration has required input files
     * Removes sources with no input files and warns about them
     * @param config MergerConfig to validate (modified in place)
     * @throws std::runtime_error if no valid sources remain
     */
    static void validateConfiguration(MergerConfig& config);

    static void validateTransformationConfiguration(
        const TransformationConfig& config);

    /**
     * Print the parsed configuration to console
     * @param config MergerConfig to print
     */
    static void printConfiguration(const MergerConfig& config);
};
