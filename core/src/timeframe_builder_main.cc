#include "EventMerger.h"
#include "DataHandler.h"
#include "CommandLineParser.h"
#include <iostream>
#include <exception>
#include <utility>

int main(int argc, char* argv[]) {
    try {
        // Parse command-line arguments and YAML configuration
        TimeframeBuilderConfig config = CommandLineParser::parse(argc, argv);
        
        // Create appropriate data handler based on output file extension
        auto data_handler = DataHandler::create(config.merger.output_file);

        // Compose the merger from run settings, application transforms, and backend
        EventMerger merger(std::move(config.merger),
                           std::move(config.transformations),
                           std::move(data_handler));
        
        // Run the merger
        merger.run();
        
        std::cout << "Successfully completed timeframe merging!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}