#pragma once

#include <podio/CollectionBuffers.h>
#include <podio/CollectionIDTable.h>
#include <podio/Frame.h>
#include <podio/GenericParameters.h>

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

using PodioCollectionBufferMap =
    std::unordered_map<std::string, podio::CollectionReadBuffers>;

/**
 * Owns the raw data needed to construct a podio::Frame for any registered
 * datamodel. Collection buffers are unpacked lazily by podio when accessed.
 */
class PodioFrameBuffers {
public:
    PodioFrameBuffers(PodioCollectionBufferMap&& collection_buffers,
                      podio::CollectionIDTable&& collection_ids,
                      podio::GenericParameters&& parameters);

    PodioFrameBuffers(const PodioFrameBuffers&) = delete;
    PodioFrameBuffers& operator=(const PodioFrameBuffers&) = delete;
    PodioFrameBuffers(PodioFrameBuffers&&) = default;
    PodioFrameBuffers& operator=(PodioFrameBuffers&&) = default;

    std::optional<podio::CollectionReadBuffers> getCollectionBuffers(
        const std::string& name);
    podio::CollectionIDTable getIDTable() const;
    std::unique_ptr<podio::GenericParameters> getParameters();
    std::vector<std::string> getAvailableCollections() const;

private:
    PodioCollectionBufferMap collection_buffers_;
    podio::CollectionIDTable collection_ids_;
    podio::GenericParameters parameters_;
};

podio::Frame buildPodioFrame(PodioFrameBuffers&& buffers);
