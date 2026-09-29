#include "PodioEDM4hepDataHandler.h"
#include <TFile.h>
#include <iostream>
#include <stdexcept>

std::vector<std::unique_ptr<DataSource>> PodioEDM4hepDataHandler::initializeDataSources(
    const std::string& filename,
    const std::vector<SourceConfig>& source_configs) {

    std::cout << "Initializing podio EDM4hep data handler for: " << filename << std::endl;

    std::vector<std::unique_ptr<DataSource>> data_sources;
    data_sources.reserve(source_configs.size());

    for (size_t i = 0; i < source_configs.size(); ++i) {
        const auto& cfg = source_configs[i];
        if (cfg.input_files.empty()) {
            throw std::runtime_error("Source " + cfg.name + " has no input files");
        }
        data_sources.push_back(std::make_unique<PodioEDM4hepDataSource>(cfg, i));
        std::cout << "Created PodioEDM4hepDataSource for: " << cfg.input_files[0] << std::endl;
    }

    // Populate the base-class edm4hep_sources_ non-owning pointer cache.
    // PodioEDM4hepDataSource inherits EDM4hepDataSource, so the cast succeeds and
    // the base-class processEvent() can call virtual process* methods on it.
    edm4hep_sources_.clear();
    edm4hep_sources_.reserve(data_sources.size());
    for (auto& src : data_sources) {
        auto* ptr = dynamic_cast<EDM4hepDataSource*>(src.get());
        if (!ptr) {
            throw std::runtime_error("Unexpected source type in PodioEDM4hepDataHandler");
        }
        edm4hep_sources_.push_back(ptr);
    }

    // Open the output file so it exists when initializeOutput() rewrites it.
    output_file_ = std::unique_ptr<TFile>(TFile::Open(filename.c_str(), "RECREATE"));
    if (!output_file_ || output_file_->IsZombie()) {
        throw std::runtime_error("Could not create output file: " + filename);
    }

    std::cout << "Podio EDM4hep data handler initialized successfully" << std::endl;

    return data_sources;
}
