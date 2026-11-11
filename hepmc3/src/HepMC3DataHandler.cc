#include "HepMC3DataHandler.h"
#include "ParameterTransforms.h"
#include <iostream>
#include <stdexcept>

namespace {

void registerHepMC3RecordFields() {
    static const bool registered = [] {
        auto& transforms = RecordTransformRegistry::instance();
        transforms.registerFieldAccessor(
            "HepMC3::GenParticle", "status", [](void* record) {
                auto* particle = static_cast<HepMC3::GenParticle*>(record);
                return NumericFieldAccessor{
                    [particle] {
                        return static_cast<double>(particle->status());
                    },
                    [particle](double value) {
                        particle->set_status(checkedNumericCast<int>(value));
                    }};
            });
        transforms.registerFieldAccessor(
            "HepMC3::GenParticle", "pid", [](void* record) {
                auto* particle = static_cast<HepMC3::GenParticle*>(record);
                return NumericFieldAccessor{
                    [particle] { return static_cast<double>(particle->pid()); },
                    [particle](double value) {
                        particle->set_pid(checkedNumericCast<int>(value));
                    }};
            });
        transforms.registerFieldAccessor(
            "HepMC3::GenParticle", "generated_mass", [](void* record) {
                auto* particle = static_cast<HepMC3::GenParticle*>(record);
                return NumericFieldAccessor{
                    [particle] {
                        return static_cast<double>(particle->generated_mass());
                    },
                    [particle](double value) {
                        particle->set_generated_mass(
                            checkedNumericCast<double>(value));
                    }};
            });
        for (const std::string component : {"px", "py", "pz", "e"}) {
            transforms.registerFieldAccessor(
                "HepMC3::GenParticle", "momentum." + component,
                [component](void* record) {
                    auto* particle = static_cast<HepMC3::GenParticle*>(record);
                    return NumericFieldAccessor{
                        [particle, component] {
                            const auto momentum = particle->momentum();
                            if (component == "px") return momentum.px();
                            if (component == "py") return momentum.py();
                            if (component == "pz") return momentum.pz();
                            return momentum.e();
                        },
                        [particle, component](double value) {
                            auto momentum = particle->momentum();
                            if (component == "px")
                                momentum.set_px(checkedNumericCast<double>(value));
                            if (component == "py")
                                momentum.set_py(checkedNumericCast<double>(value));
                            if (component == "pz")
                                momentum.set_pz(checkedNumericCast<double>(value));
                            if (component == "e")
                                momentum.set_e(checkedNumericCast<double>(value));
                            particle->set_momentum(momentum);
                        }};
                });
        }
        for (const std::string component : {"x", "y", "z", "t"}) {
            transforms.registerFieldAccessor(
                "HepMC3::GenVertex", "position." + component,
                [component](void* record) {
                    auto* vertex = static_cast<HepMC3::GenVertex*>(record);
                    return NumericFieldAccessor{
                        [vertex, component] {
                            const auto position = vertex->position();
                            if (component == "x") return position.x();
                            if (component == "y") return position.y();
                            if (component == "z") return position.z();
                            return position.t();
                        },
                        [vertex, component](double value) {
                            auto position = vertex->position();
                            if (component == "x")
                                position.set_x(checkedNumericCast<double>(value));
                            if (component == "y")
                                position.set_y(checkedNumericCast<double>(value));
                            if (component == "z")
                                position.set_z(checkedNumericCast<double>(value));
                            if (component == "t")
                                position.set_t(checkedNumericCast<double>(value));
                            vertex->set_position(position);
                        }};
                });
        }
        return true;
    }();
    (void)registered;
}

} // namespace

std::vector<std::unique_ptr<DataSource>> HepMC3DataHandler::initializeDataSources(
    const std::string& filename,
    const std::vector<SourceConfig>& source_configs) {

    std::cout << "Initializing HepMC3 data handler for: " << filename << std::endl;
    
    std::vector<std::unique_ptr<DataSource>> data_sources;
    data_sources.reserve(source_configs.size());
    
    // Create HepMC3DataSource objects for each source configuration
    for (size_t source_idx = 0; source_idx < source_configs.size(); ++source_idx) {
        const auto& source_config = source_configs[source_idx];
        
        // Determine format from first input file
        if (source_config.input_files.empty()) {
            throw std::runtime_error("Source " + source_config.name + " has no input files");
        }
        
        const std::string& first_file = source_config.input_files[0];
        
        // Helper lambda to check file extension
        auto hasExtension = [](const std::string& filename, const std::string& ext) {
            if (filename.length() < ext.length()) return false;
            return filename.compare(filename.length() - ext.length(), ext.length(), ext) == 0;
        };
        
        // Check if this is HepMC3 format
        if (!hasExtension(first_file, ".hepmc3.tree.root")) {
            throw std::runtime_error(
                "HepMC3DataHandler can only handle .hepmc3.tree.root files. "
                "Got: " + first_file
            );
        }
        
        auto data_source = std::make_unique<HepMC3DataSource>(source_config, source_idx);
        std::cout << "Created HepMC3DataSource name: " + source_config.name + " for: " + first_file << std::endl;
        data_sources.push_back(std::move(data_source));
    }
    
    // Store pointers to HepMC3 sources for later use
    hepmc3_sources_.clear();
    hepmc3_sources_.reserve(data_sources.size());
    for (auto& source : data_sources) {
        hepmc3_sources_.push_back(dynamic_cast<HepMC3DataSource*>(source.get()));
    }
    
    std::cout << "HepMC3 data handler initialized with " << hepmc3_sources_.size() << " sources" << std::endl;
    
    return data_sources;
}

void HepMC3DataHandler::initializeOutput(
    const MergerConfig& config,
    const TransformationConfig& transformations,
    const std::vector<std::unique_ptr<DataSource>>& data_sources) {
    event_transform_rules_ = transformations.event_transformations;
    record_transform_rules_ = transformations.record_transformations;
    registerHepMC3RecordFields();
    EventTransformRegistry::instance().validateRules(event_transform_rules_);
    RecordTransformRegistry::instance().validateRules(record_transform_rules_);

    // Configure metadata and create writer
    auto runInfo = configureMetadata(config);
    writer_ = std::make_shared<HepMC3::WriterRootTree>(config.output_file, runInfo);
    if (!writer_) {
        throw std::runtime_error("Failed to create HepMC3 writer for: " + config.output_file);
    }
}

std::shared_ptr<HepMC3::GenRunInfo> HepMC3DataHandler::configureMetadata(const MergerConfig& config) {

    auto runInfo = std::make_shared<HepMC3::GenRunInfo>();

    std::string metadata_prefix = "TimeframeBuilder_HepMC3_";

    // Fix me when release version exists
    runInfo->add_attribute(metadata_prefix + "version", std::make_shared<HepMC3::StringAttribute>(TIMEFRAME_BUILDER_VERSION_STR));

    runInfo->add_attribute(metadata_prefix + "timeframe_duration_ns", std::make_shared<HepMC3::StringAttribute>(std::to_string(config.timeframe_duration)));
    runInfo->add_attribute(metadata_prefix + "bunch_crossing_period_ns", std::make_shared<HepMC3::StringAttribute>(std::to_string(config.bunch_crossing_period)));
    runInfo->add_attribute(metadata_prefix + "max_events", std::make_shared<HepMC3::StringAttribute>(std::to_string(config.max_events)));
    runInfo->add_attribute(metadata_prefix + "random_seed", std::make_shared<HepMC3::StringAttribute>(std::to_string(config.random_seed)));

    runInfo->add_attribute(metadata_prefix + "number_of_sources", std::make_shared<HepMC3::StringAttribute>(std::to_string(config.sources.size())));

    for (const auto& source_config : config.sources) {
        std::string source_prefix = metadata_prefix + "source_" + source_config.name + "_";
        runInfo->add_attribute(source_prefix + "static_number_of_events", std::make_shared<HepMC3::StringAttribute>(source_config.static_number_of_events ? "true" : "false"));
        if(source_config.static_number_of_events) {
            runInfo->add_attribute(source_prefix + "static_events_per_timeframe", std::make_shared<HepMC3::StringAttribute>(std::to_string(source_config.static_events_per_timeframe)));
        } else{
            runInfo->add_attribute(source_prefix + "mean_event_frequency_GHz", std::make_shared<HepMC3::StringAttribute>(std::to_string(source_config.mean_event_frequency)));
        }
        runInfo->add_attribute(source_prefix + "input_files", std::make_shared<HepMC3::StringAttribute>(std::to_string(source_config.input_files.size())));
        for (size_t i = 0; i < source_config.input_files.size(); ++i) {
            runInfo->add_attribute(source_prefix + "input_file_" + std::to_string(i), std::make_shared<HepMC3::StringAttribute>(source_config.input_files[i]));
        }
        runInfo->add_attribute(source_prefix + "generator_status_offset", std::make_shared<HepMC3::StringAttribute>(std::to_string(source_config.generator_status_offset)));
        runInfo->add_attribute(source_prefix + "repeat_on_end_of_file", std::make_shared<HepMC3::StringAttribute>(source_config.repeat_on_eof ? "true" : "false"));
        runInfo->add_attribute(source_prefix + "skipped_events", std::make_shared<HepMC3::StringAttribute>(std::to_string(source_config.skip)));
    }
    
    return runInfo;
}

void HepMC3DataHandler::prepareTimeframe() {
    // Create a new empty event for this timeframe
    current_timeframe_ = std::make_unique<HepMC3::GenEvent>(
        HepMC3::Units::GEV,
        HepMC3::Units::MM
    );
}

void HepMC3DataHandler::processEvent(DataSource& source) {
    auto* hepmc3_source = dynamic_cast<HepMC3DataSource*>(&source);
    if (!hepmc3_source) {
        throw std::runtime_error("HepMC3DataHandler: Expected HepMC3DataSource");
    }
    
    const auto& config = source.getConfig();
    // Get the current event from the HepMC3 source
    const auto& event = hepmc3_source->getCurrentEvent();
    
    // Insert the event into the merged timeframe
    insertHepMC3Event(event, current_timeframe_, config.keep_weight);
}

long HepMC3DataHandler::insertHepMC3Event(const HepMC3::GenEvent& inevt,
                                            std::unique_ptr<HepMC3::GenEvent>& hepframe,
                                            bool keepWeights) {
    std::vector<HepMC3::GenParticlePtr> particles;
    std::vector<HepMC3::GenVertexPtr> vertices;

    // Copy vertices and apply configured record transformations.
    for (const auto& vertex : inevt.vertices()) {
        auto v1 = std::make_shared<HepMC3::GenVertex>(vertex->position());
        RecordTransformRegistry::instance().apply(
            "HepMC3::GenVertex", "vertices", v1.get(),
            record_transform_rules_, current_event_transform_context_);
        vertices.push_back(v1);
    }
    
    // Copy particles and attach them to their corresponding vertices
    long finalParticleCount = 0;
    for (const auto& particle : inevt.particles()) {
        HepMC3::FourVector momentum = particle->momentum();
        int status = particle->status();
        if (status == 1) finalParticleCount++;
        int pid = particle->pid();
        auto p1 = std::make_shared<HepMC3::GenParticle>(momentum, pid, status);
        p1->set_generated_mass(particle->generated_mass());
        RecordTransformRegistry::instance().apply(
            "HepMC3::GenParticle", "particles", p1.get(),
            record_transform_rules_, current_event_transform_context_);
        particles.push_back(p1);
        
        // Attach particle to production vertex with bounds checking
        if (particle->production_vertex() && particle->production_vertex()->id() < 0) {
            int production_vertex = particle->production_vertex()->id();
            size_t vertex_index = abs(production_vertex) - 1;
            if (vertex_index < vertices.size()) {
                vertices[vertex_index]->add_particle_out(p1);
                hepframe->add_particle(p1);
            } else {
                std::cerr << "Warning: Invalid production vertex index " << vertex_index 
                          << " (vertices size: " << vertices.size() << ")" << std::endl;
            }
        }
        
        // Attach particle to end vertex if it has one with bounds checking
        if (particle->end_vertex()) {
            int end_vertex = particle->end_vertex()->id();
            size_t vertex_index = abs(end_vertex) - 1;
            if (vertex_index < vertices.size()) {
                vertices[vertex_index]->add_particle_in(p1);
            } else {
                std::cerr << "Warning: Invalid end vertex index " << vertex_index 
                          << " (vertices size: " << vertices.size() << ")" << std::endl;
            }
        }
    }

    // Add all vertices to the merged event
    for (auto& vertex : vertices) {
        hepframe->add_vertex(vertex);
    }

    if(keepWeights) {
        // Copy weights from the original event
        for (const auto& weight : inevt.weights()) {
            hepframe->weights().push_back(weight);
        }
    }
    
    return finalParticleCount;
}

void HepMC3DataHandler::writeTimeframe() {
    if (!current_timeframe_) {
        throw std::runtime_error("No timeframe to write - prepareTimeframe() not called?");
    }
    
    // Set event number
    current_timeframe_->set_event_number(current_timeframe_number_);
    
    // Write the event
    writer_->write_event(*current_timeframe_);
    
    // Clear the timeframe
    current_timeframe_.reset();
}

void HepMC3DataHandler::finalize() {
    if (writer_) {
        writer_->close();
        writer_.reset();
    }
    std::cout << "HepMC3 output finalized" << std::endl;
}
