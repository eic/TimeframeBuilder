#include "TimeframeBuilder.h"
#include "DataHandler.h"
#include "CommandLineParser.h"
#include <iostream>
#include <exception>

int main(int argc, char* argv[]) {
    try {
        // Parse command-line arguments and YAML configuration
        MergerConfig config = CommandLineParser::parse(argc, argv);
        
        // Create the merger
        TimeframeBuilder merger(config);
        
        // Create appropriate data handler based on output file extension and reader config
        auto data_handler = DataHandler::create(config);
        merger.setDataHandler(std::move(data_handler));
        
        // Run the merger
        merger.run();
        
        std::cout << "Successfully completed timeframe merging!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}