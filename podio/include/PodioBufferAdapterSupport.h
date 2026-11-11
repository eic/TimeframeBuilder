#pragma once

#include "ParameterTransforms.h"
#include "PodioBufferMerger.h"

#include <podio/CollectionBuffers.h>
#include <podio/CollectionIDTable.h>

#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

void remapAndAppendPodioReferences(
    const std::string& collection_name, podio::CollectionReadBuffers& source,
    podio::CollectionReadBuffers& destination,
    const podio::CollectionIDTable& source_ids,
    const podio::CollectionIDTable& destination_ids,
    const PodioBufferMerger::CollectionSizes& collection_sizes);

template <typename Integer>
void addPodioRangeOffset(Integer& value, size_t offset,
                         const std::string& collection_name) {
    static_assert(std::is_integral_v<Integer> && std::is_unsigned_v<Integer>);
    if (offset > std::numeric_limits<Integer>::max() ||
        value > std::numeric_limits<Integer>::max() -
                    static_cast<Integer>(offset)) {
        throw std::overflow_error(
            "Merged relation range exceeds field range in collection '" +
            collection_name + "'");
    }
    value += static_cast<Integer>(offset);
}

struct PodioVectorMemberAdapter {
    std::function<size_t(podio::CollectionReadBuffers&)> size;
    std::function<void(podio::CollectionReadBuffers&,
                       podio::CollectionReadBuffers&)>
        append;
};

template <typename Value>
PodioVectorMemberAdapter podioVectorMemberAdapter(size_t index,
                                                   std::string type_name) {
    auto get_values = [index, type_name](podio::CollectionReadBuffers& buffers)
        -> std::vector<Value>& {
        if (!buffers.vectorMembers || index >= buffers.vectorMembers->size() ||
            (*buffers.vectorMembers)[index].first != type_name ||
            !(*buffers.vectorMembers)[index].second) {
            throw std::runtime_error("Vector-member buffer layout mismatch");
        }
        return *static_cast<std::vector<Value>*>(
            (*buffers.vectorMembers)[index].second);
    };

    return {
        [get_values](podio::CollectionReadBuffers& buffers) {
            return get_values(buffers).size();
        },
        [get_values](podio::CollectionReadBuffers& source,
                     podio::CollectionReadBuffers& destination) {
            auto& source_values = get_values(source);
            auto& destination_values = get_values(destination);
            destination_values.insert(destination_values.end(),
                                      source_values.begin(),
                                      source_values.end());
        }};
}

template <typename Data>
void registerPodioBufferAdapter(
    const std::string& collection_type, const std::string& datatype,
    size_t relation_count,
    std::function<void(Data&, const std::vector<size_t>&,
                       const std::vector<size_t>&)>
        adjust_ranges,
    std::vector<PodioVectorMemberAdapter> vector_members = {}) {
    auto merge = [datatype, relation_count, adjust_ranges,
                  vector_members = std::move(vector_members)](
                     const std::string& collection_name,
                     podio::CollectionReadBuffers& source,
                     podio::CollectionReadBuffers& destination,
                     const podio::CollectionIDTable& source_ids,
                     const podio::CollectionIDTable& destination_ids,
                     const PodioBufferMerger::CollectionSizes& collection_sizes,
                     const std::vector<RecordTransformRule>& rules,
                     const EventTransformContext& context) {
        auto* source_data = source.dataAsVector<Data>();
        auto* destination_data = destination.dataAsVector<Data>();
        if (!source_data || !destination_data) {
            throw std::runtime_error("Missing data buffer for collection '" +
                                     collection_name + "'");
        }
        if (!source.references || !destination.references ||
            source.references->size() != relation_count ||
            destination.references->size() != relation_count) {
            throw std::runtime_error("Relation buffer layout mismatch for collection '" +
                                     collection_name + "'");
        }

        std::vector<size_t> relation_offsets;
        relation_offsets.reserve(relation_count);
        for (const auto& refs : *destination.references) {
            if (!refs) {
                throw std::runtime_error(
                    "Null destination relation buffer in collection '" +
                    collection_name + "'");
            }
            relation_offsets.push_back(refs->size());
        }

        if (!source.vectorMembers || !destination.vectorMembers ||
            source.vectorMembers->size() != vector_members.size() ||
            destination.vectorMembers->size() != vector_members.size()) {
            throw std::runtime_error(
                "Vector-member buffer layout mismatch for collection '" +
                collection_name + "'");
        }
        std::vector<size_t> member_offsets;
        member_offsets.reserve(vector_members.size());
        for (const auto& member : vector_members) {
            member_offsets.push_back(member.size(destination));
        }

        for (auto& data : *source_data) {
            RecordTransformRegistry::instance().apply(
                datatype, collection_name, &data, rules, context);
            adjust_ranges(data, relation_offsets, member_offsets);
        }
        remapAndAppendPodioReferences(
            collection_name, source, destination, source_ids, destination_ids,
            collection_sizes);
        for (const auto& member : vector_members) {
            member.append(source, destination);
        }
        destination_data->insert(destination_data->end(), source_data->begin(),
                                 source_data->end());
    };

    auto size = [](podio::CollectionReadBuffers& buffers) {
        auto* data = buffers.dataAsVector<Data>();
        if (!data) {
            throw std::runtime_error("Missing collection data buffer");
        }
        return data->size();
    };
    PodioBufferMerger::instance().registerAdapter(collection_type,
                                                  std::move(merge),
                                                  std::move(size));
}

template <typename Data>
void registerPodioBeamVertexAdapter(
    const std::string& collection_type,
    std::function<std::optional<PodioBufferMerger::BeamVertex>(
        const std::vector<Data>&)>
        get_vertex) {
    PodioBufferMerger::instance().registerBeamVertexAdapter(
        collection_type,
        [get_vertex = std::move(get_vertex)](
            const podio::CollectionReadBuffers& buffers) {
            auto* values =
                podio::CollectionReadBuffers::asVector<Data>(buffers.data);
            if (!values) {
                throw std::runtime_error(
                    "Missing data buffer while finding beam vertex");
            }
            return get_vertex(*values);
        });
}

void registerPodioEventHeaderAdapter(
    const std::string& collection_type,
    PodioBufferMerger::EventHeaderFunction append_header);
