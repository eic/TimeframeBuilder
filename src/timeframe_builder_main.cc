#include "CommandLineParser.h"
#include "DataHandler.h"
#include "TimeframeBuilder.h"
#include <exception>
#include <iostream>

int main(int argc, char *argv[]) {
  try {
    // Parse command-line arguments and YAML configuration
    MergerConfig config = CommandLineParser::parse(argc, argv);

    // Create the merger
    TimeframeBuilder merger(config);

    // Create appropriate data handler based on output file extension
    auto data_handler = DataHandler::create(config.output_file);
    merger.setDataHandler(std::move(data_handler));

    // Run the merger
    merger.run();

    std::cout << "Successfully completed timeframe merging!" << std::endl;
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}
