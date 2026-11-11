#include "PodioBufferAdapterSupport.h"

#include <podio/ObjectID.h>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {

uint32_t remapCollectionID(const podio::ObjectID& reference,
                           const podio::CollectionIDTable& source_ids,
                           const podio::CollectionIDTable& destination_ids,
                           const std::string& collection_name) {
    if (reference.collectionID ==
            static_cast<uint32_t>(podio::ObjectID::untracked) ||
        reference.collectionID ==
            static_cast<uint32_t>(podio::ObjectID::invalid)) {
        return reference.collectionID;
    }
    const auto name = source_ids.name(reference.collectionID);
    if (!name) {
        throw std::runtime_error("Reference in collection '" + collection_name +
                                 "' uses an unknown source collection ID");
    }
    const auto id = destination_ids.collectionID(*name);
    if (!id) {
        throw std::runtime_error("Reference in collection '" + collection_name +
                                 "' points to collection '" + *name +
                                 "' that is not in the output frame");
    }
    return *id;
}

} // namespace

void remapAndAppendPodioReferences(
    const std::string& collection_name, podio::CollectionReadBuffers& source,
    podio::CollectionReadBuffers& destination,
    const podio::CollectionIDTable& source_ids,
    const podio::CollectionIDTable& destination_ids,
    const PodioBufferMerger::CollectionSizes& collection_sizes) {
    if (!source.references || !destination.references ||
        source.references->size() != destination.references->size()) {
        throw std::runtime_error("Relation buffer layout mismatch for collection '" +
                                 collection_name + "'");
    }

    for (size_t relation_index = 0;
         relation_index < source.references->size(); ++relation_index) {
        if (!(*source.references)[relation_index] ||
            !(*destination.references)[relation_index]) {
            throw std::runtime_error(
                "Null relation buffer in collection '" + collection_name + "'");
        }
        auto& source_refs = *(*source.references)[relation_index];
        auto& destination_refs = *(*destination.references)[relation_index];
        for (auto& reference : source_refs) {
            reference.collectionID = remapCollectionID(
                reference, source_ids, destination_ids, collection_name);
            if (reference.index < 0) {
                continue;
            }
            const auto target_name = destination_ids.name(reference.collectionID);
            if (!target_name) {
                throw std::runtime_error(
                    "Remapped reference has no output collection name");
            }
            const auto offset = collection_sizes.find(*target_name);
            if (offset == collection_sizes.end()) {
                throw std::runtime_error(
                    "No output collection offset is available for reference target '" +
                    *target_name + "'");
            }
            if (offset->second >
                    static_cast<size_t>(std::numeric_limits<int>::max()) ||
                reference.index >
                    std::numeric_limits<int>::max() -
                        static_cast<int>(offset->second)) {
                throw std::overflow_error(
                    "Merged reference index exceeds podio::ObjectID range");
            }
            reference.index += static_cast<int>(offset->second);
        }
        destination_refs.insert(destination_refs.end(), source_refs.begin(),
                                source_refs.end());
    }
}

void registerPodioEventHeaderAdapter(
    const std::string& collection_type,
    PodioBufferMerger::EventHeaderFunction append_header) {
    PodioBufferMerger::instance().registerEventHeaderAdapter(
        collection_type, std::move(append_header));
}
