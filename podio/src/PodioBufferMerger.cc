#include "PodioBufferMerger.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

PodioBufferMerger& PodioBufferMerger::instance() {
    static PodioBufferMerger merger;
    return merger;
}

void PodioBufferMerger::registerAdapter(std::string type, MergeFunction merge,
                                        SizeFunction size) {
    if (type.empty() || !merge || !size) {
        throw std::invalid_argument(
            "Podio buffer adapters require a type and both operations");
    }
    if (!adapters_.emplace(std::move(type),
                           Adapter{std::move(merge), std::move(size)})
             .second) {
        throw std::invalid_argument("Duplicate podio buffer adapter");
    }
}

void PodioBufferMerger::registerBeamVertexAdapter(
    std::string type, BeamVertexFunction get_vertex) {
    if (type.empty() || !get_vertex) {
        throw std::invalid_argument(
            "Podio beam-vertex adapters require a type and accessor");
    }
    if (!beam_vertex_adapters_
             .emplace(std::move(type), std::move(get_vertex))
             .second) {
        throw std::invalid_argument("Duplicate podio beam-vertex adapter");
    }
}

void PodioBufferMerger::registerEventHeaderAdapter(
    std::string type, EventHeaderFunction append_header) {
    if (type.empty() || !append_header) {
        throw std::invalid_argument(
            "Podio event-header adapters require a type and appender");
    }
    if (!event_header_adapters_
             .emplace(std::move(type), std::move(append_header))
             .second) {
        throw std::invalid_argument("Duplicate podio event-header adapter");
    }
}

bool PodioBufferMerger::hasAdapter(const std::string& type) const {
    return adapters_.contains(type);
}

size_t PodioBufferMerger::collectionSize(
    podio::CollectionReadBuffers& buffers) const {
    const auto adapter = adapters_.find(std::string(buffers.type));
    if (adapter == adapters_.end()) {
        throw std::runtime_error("No buffer merge adapter for podio type '" +
                                 std::string(buffers.type) + "'");
    }
    return adapter->second.size(buffers);
}

std::optional<PodioBufferMerger::BeamVertex> PodioBufferMerger::beamVertex(
    const podio::CollectionReadBuffers& buffers) const {
    const auto adapter = beam_vertex_adapters_.find(std::string(buffers.type));
    if (adapter == beam_vertex_adapters_.end()) {
        throw std::runtime_error(
            "No generated beam-vertex adapter for podio type '" +
            std::string(buffers.type) + "'");
    }
    return adapter->second(buffers);
}

bool PodioBufferMerger::hasEventHeaderAdapter(const std::string& type) const {
    return event_header_adapters_.contains(type);
}

void PodioBufferMerger::appendEventHeader(
    const std::string& type, podio::CollectionReadBuffers& buffers,
    const EventHeaderValues& values) const {
    const auto adapter = event_header_adapters_.find(type);
    if (adapter == event_header_adapters_.end()) {
        throw std::runtime_error(
            "No generated event-header adapter for podio type '" + type + "'");
    }
    adapter->second(buffers, values);
}

void PodioBufferMerger::merge(
    const std::string& collection_name,
    podio::CollectionReadBuffers& source,
    podio::CollectionReadBuffers& destination,
    const podio::CollectionIDTable& source_ids,
    const podio::CollectionIDTable& destination_ids,
    const CollectionSizes& collection_sizes,
    const std::vector<RecordTransformRule>& rules,
    const EventTransformContext& context) const {
    const auto adapter = adapters_.find(std::string(source.type));
    if (adapter == adapters_.end()) {
        throw std::runtime_error("No buffer merge adapter for podio type '" +
                                 std::string(source.type) + "' (collection '" +
                                 collection_name + "')");
    }
    if (source.type != destination.type ||
        source.schemaVersion != destination.schemaVersion) {
        throw std::runtime_error("Incompatible podio collection schema for '" +
                                 collection_name + "'");
    }
    adapter->second.merge(collection_name, source, destination, source_ids,
                          destination_ids, collection_sizes, rules, context);
}
