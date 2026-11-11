#include "PodioDataHandler.h"

#include "ParameterTransforms.h"
#include "PodioBufferOwnership.h"
#include "PodioDataSource.h"
#include "PodioFrameBuilder.h"

#include <podio/CollectionBufferFactory.h>

#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>

PodioDataHandler::~PodioDataHandler() {
    releasePodioCollectionBuffers(collection_buffers_);
}

std::vector<std::unique_ptr<DataSource>>
PodioDataHandler::initializeDataSources(
    const std::string&, const std::vector<SourceConfig>& source_configs) {
    std::vector<std::unique_ptr<DataSource>> sources;
    sources.reserve(source_configs.size());
    for (size_t i = 0; i < source_configs.size(); ++i) {
        sources.push_back(
            std::make_unique<PodioDataSource>(source_configs[i], i));
    }
    return sources;
}

void PodioDataHandler::initializeOutput(
    const MergerConfig& config,
    const TransformationConfig& transformations,
    const std::vector<std::unique_ptr<DataSource>>& data_sources) {
    event_transform_rules_ = transformations.event_transformations;
    record_transform_rules_ = transformations.record_transformations;
    registerGeneratedPodioAdapters();
    EventTransformRegistry::instance().validateRules(event_transform_rules_);
    RecordTransformRegistry::instance().validateRules(record_transform_rules_);

    for (const auto& source : data_sources) {
        source->initialize({}, {}, {});
        auto* podio_source = dynamic_cast<PodioDataSource*>(source.get());
        if (!podio_source) {
            throw std::runtime_error(
                "PodioDataHandler requires PodioDataSource input sources");
        }
        podio_source->loadEvent(podio_source->getCurrentEntryIndex());
        for (const auto& [name, buffers] :
             podio_source->getCurrentCollectionBuffers()) {
            const std::string type{buffers.type};
            if (!PodioBufferMerger::instance().hasAdapter(type)) {
                throw std::runtime_error(
                    "No buffer merge adapter for podio type '" + type +
                    "' (collection '" + name + "')");
            }
            const auto [schema, inserted] = collection_schema_.emplace(
                name, CollectionSchema{type, buffers.schemaVersion});
            if (!inserted &&
                (schema->second.type != type ||
                 schema->second.version != buffers.schemaVersion)) {
                throw std::runtime_error(
                    "Input collection schema differs between sources for '" +
                    name + "'");
            }
        }
    }

    const auto event_header = collection_schema_.find("EventHeader");
    if (event_header != collection_schema_.end() &&
        PodioBufferMerger::instance().hasEventHeaderAdapter(
            event_header->second.type)) {
        event_header_type_ = event_header->second.type;
        const bool has_unmerged_source =
            std::any_of(data_sources.begin(), data_sources.end(),
                        [](const auto& source) {
                            return !source->getConfig().already_merged;
                        });
        if (has_unmerged_source) {
            const auto [sub_event_header, inserted] =
                collection_schema_.emplace("SubEventHeaders",
                                           event_header->second);
            if (!inserted &&
                (sub_event_header->second.type != event_header->second.type ||
                 sub_event_header->second.version !=
                     event_header->second.version)) {
                throw std::runtime_error(
                    "SubEventHeaders has a schema incompatible with EventHeader");
            }
        }
    }

    writer_ = std::make_unique<podio::ROOTWriter>(config.output_file);
    timeframe_parameters_.reset();
}

void PodioDataHandler::prepareTimeframe() {
    releasePodioCollectionBuffers(collection_buffers_);
    collection_ids_ = podio::CollectionIDTable{};
    timeframe_parameters_.reset();
    createEmptyCollections();
}

void PodioDataHandler::createEmptyCollections() {
    for (const auto& [name, schema] : collection_schema_) {
        auto buffers = podio::CollectionBufferFactory::instance().createBuffers(
            schema.type, schema.version, false);
        if (!buffers) {
            throw std::runtime_error(
                "Could not create empty buffers for collection '" + name +
                "' of type '" + schema.type + "'");
        }
        collection_ids_.add(name);
        collection_buffers_.emplace(name, std::move(*buffers));
    }
}

void PodioDataHandler::processEvent(DataSource& source) {
    auto* podio_source = dynamic_cast<PodioDataSource*>(&source);
    if (!podio_source) {
        throw std::runtime_error(
            "PodioDataHandler requires PodioDataSource input sources");
    }

    auto input_buffers = podio_source->takeCurrentCollectionBuffers();
    struct InputBufferGuard {
        PodioCollectionBufferMap& buffers;
        ~InputBufferGuard() { releasePodioCollectionBuffers(buffers); }
    } input_buffer_guard{input_buffers};
    auto frame_data = podio_source->takeCurrentFrameData();
    auto source_ids = frame_data->getIDTable();
    std::vector<std::string> names;
    names.reserve(input_buffers.size());
    for (const auto& [name, _] : input_buffers) {
        names.push_back(name);
    }

    for (const auto& name : source_ids.names()) {
        if (std::find(names.begin(), names.end(), name) == names.end()) {
            throw std::runtime_error("Source frame has no readable buffers for collection '" +
                                     name + "'");
        }
        collection_ids_.add(name);
    }
    const auto output_ids = podio::CollectionIDTable{
        collection_ids_.ids(), collection_ids_.names()};

    PodioBufferMerger::CollectionSizes collection_sizes;
    for (auto& [name, buffers] : collection_buffers_) {
        collection_sizes[name] =
            PodioBufferMerger::instance().collectionSize(buffers);
    }

    const auto& source_config = podio_source->getConfig();
    const auto& context = current_event_transform_context_;

    for (const auto& name : names) {
        if (event_header_type_ && name == "EventHeader") {
            continue;
        }
        if (event_header_type_ && name == "SubEventHeaders" &&
            !source_config.already_merged) {
            continue;
        }
        auto source_buffer_it = input_buffers.find(name);
        if (source_buffer_it == input_buffers.end()) {
            throw std::runtime_error("Missing collection buffers for '" + name +
                                     "' in source '" + source.getName() + "'");
        }
        auto& source_buffer = source_buffer_it->second;

        const std::string type{source_buffer.type};
        if (!PodioBufferMerger::instance().hasAdapter(type)) {
            throw std::runtime_error("No buffer merge adapter for podio type '" +
                                     type + "' (collection '" + name + "')");
        }

        auto schema = collection_schema_.find(name);
        if (schema == collection_schema_.end()) {
            throw std::runtime_error(
                "Collection '" + name +
                "' was not present when the podio output schema was initialized");
        }

        if (schema->second.type != type ||
            schema->second.version != source_buffer.schemaVersion) {
            throw std::runtime_error(
                "Input collection schema differs from the established output schema for '" +
                name + "'");
        }

        auto destination = collection_buffers_.find(name);
        if (destination == collection_buffers_.end()) {
            throw std::runtime_error(
                "Output buffers were not initialized for collection '" + name +
                "'");
        }
        PodioBufferMerger::instance().merge(
            name, source_buffer, destination->second, source_ids, output_ids,
            collection_sizes, record_transform_rules_, context);
    }

    if (event_header_type_ && !source_config.already_merged) {
        const auto sub_event_headers = collection_buffers_.find("SubEventHeaders");
        if (sub_event_headers == collection_buffers_.end()) {
            throw std::runtime_error(
                "SubEventHeaders output buffers were not initialized");
        }
        const auto particle_offset = collection_sizes.find("MCParticles");
        const auto particle_index =
            particle_offset == collection_sizes.end() ? size_t{0}
                                                       : particle_offset->second;
        if (source.getSourceIndex() >
            std::numeric_limits<uint32_t>::max()) {
            throw std::overflow_error("Source index exceeds EventHeader runNumber");
        }
        PodioBufferMerger::instance().appendEventHeader(
            *event_header_type_, sub_event_headers->second,
            {total_events_consumed_,
             static_cast<uint32_t>(source.getSourceIndex()),
             static_cast<uint64_t>(particle_index),
             context.number("time_offset")});
    }
    ++total_events_consumed_;

    if (!timeframe_parameters_) {
        timeframe_parameters_ = frame_data->getParameters();
    } else {
        (void)frame_data->getParameters();
    }
}

void PodioDataHandler::writeTimeframe() {
    if (!writer_) {
        throw std::logic_error("Podio output writer is not initialized");
    }
    if (!timeframe_parameters_) {
        timeframe_parameters_ = std::make_unique<podio::GenericParameters>();
    }
    if (event_header_type_) {
        const auto event_headers = collection_buffers_.find("EventHeader");
        if (event_headers == collection_buffers_.end()) {
            throw std::runtime_error(
                "EventHeader output buffers were not initialized");
        }
        PodioBufferMerger::instance().appendEventHeader(
            *event_header_type_, event_headers->second,
            {static_cast<uint64_t>(current_timeframe_number_), 0,
             static_cast<uint64_t>(current_timeframe_number_), 0.0});
    }

    PodioFrameBuffers frame_data{
        std::move(collection_buffers_), std::move(collection_ids_),
        std::move(*timeframe_parameters_)};
    auto frame = buildPodioFrame(std::move(frame_data));
    writer_->writeFrame(frame, "events");
    std::cout << "Wrote podio timeframe " << current_timeframe_number_
              << std::endl;
}

void PodioDataHandler::finalize() {
    if (writer_) {
        writer_->finish();
        writer_.reset();
    }
}
