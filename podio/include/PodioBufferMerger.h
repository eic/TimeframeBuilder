#pragma once

#include "ParameterTransforms.h"

#include <podio/CollectionBuffers.h>
#include <podio/CollectionIDTable.h>

#include <cstddef>
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class PodioBufferMerger {
public:
    using CollectionSizes = std::unordered_map<std::string, size_t>;
    using MergeFunction = std::function<void(
        const std::string&, podio::CollectionReadBuffers&,
        podio::CollectionReadBuffers&, const podio::CollectionIDTable&,
        const podio::CollectionIDTable&, const CollectionSizes&,
        const std::vector<RecordTransformRule>&,
        const EventTransformContext&)>;
    using SizeFunction =
        std::function<size_t(podio::CollectionReadBuffers&)>;
    using BeamVertex = std::array<float, 3>;
    using BeamVertexFunction =
        std::function<std::optional<BeamVertex>(
            const podio::CollectionReadBuffers&)>;
    struct EventHeaderValues {
        uint64_t event_number{0};
        uint32_t run_number{0};
        uint64_t time_stamp{0};
        double weight{0.0};
    };
    using EventHeaderFunction =
        std::function<void(podio::CollectionReadBuffers&,
                           const EventHeaderValues&)>;

    static PodioBufferMerger& instance();

    void registerAdapter(std::string type, MergeFunction merge,
                         SizeFunction size);
    void registerBeamVertexAdapter(std::string type,
                                   BeamVertexFunction get_vertex);
    void registerEventHeaderAdapter(std::string type,
                                    EventHeaderFunction append_header);

    bool hasAdapter(const std::string& type) const;
    size_t collectionSize(podio::CollectionReadBuffers& buffers) const;
    std::optional<BeamVertex> beamVertex(
        const podio::CollectionReadBuffers& buffers) const;
    bool hasEventHeaderAdapter(const std::string& type) const;
    void appendEventHeader(const std::string& type,
                           podio::CollectionReadBuffers& buffers,
                           const EventHeaderValues& values) const;

    void merge(const std::string& collection_name,
               podio::CollectionReadBuffers& source,
               podio::CollectionReadBuffers& destination,
               const podio::CollectionIDTable& source_ids,
               const podio::CollectionIDTable& destination_ids,
               const CollectionSizes& collection_sizes,
               const std::vector<RecordTransformRule>& rules,
               const EventTransformContext& context) const;

private:
    struct Adapter {
        MergeFunction merge;
        SizeFunction size;
    };

    std::unordered_map<std::string, Adapter> adapters_;
    std::unordered_map<std::string, BeamVertexFunction> beam_vertex_adapters_;
    std::unordered_map<std::string, EventHeaderFunction> event_header_adapters_;
};

void registerGeneratedPodioAdapters();
