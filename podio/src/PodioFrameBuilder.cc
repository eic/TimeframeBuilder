#include "PodioFrameBuilder.h"

#include <utility>

PodioFrameBuffers::PodioFrameBuffers(
    PodioCollectionBufferMap&& collection_buffers,
    podio::CollectionIDTable&& collection_ids,
    podio::GenericParameters&& parameters) :
    collection_buffers_(std::move(collection_buffers)),
    collection_ids_(std::move(collection_ids)),
    parameters_(std::move(parameters)) {
}

std::optional<podio::CollectionReadBuffers>
PodioFrameBuffers::getCollectionBuffers(const std::string& name) {
    auto buffer = collection_buffers_.extract(name);
    if (buffer.empty()) {
        return std::nullopt;
    }

    return std::move(buffer.mapped());
}

podio::CollectionIDTable PodioFrameBuffers::getIDTable() const {
    return {collection_ids_.ids(), collection_ids_.names()};
}

std::unique_ptr<podio::GenericParameters> PodioFrameBuffers::getParameters() {
    return std::make_unique<podio::GenericParameters>(std::move(parameters_));
}

std::vector<std::string> PodioFrameBuffers::getAvailableCollections() const {
    std::vector<std::string> names;
    names.reserve(collection_buffers_.size());
    for (const auto& [name, _] : collection_buffers_) {
        names.push_back(name);
    }
    return names;
}

podio::Frame buildPodioFrame(PodioFrameBuffers&& buffers) {
    return podio::Frame{
        std::make_unique<PodioFrameBuffers>(std::move(buffers))};
}
