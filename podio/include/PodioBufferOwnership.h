#pragma once

#include <podio/CollectionBuffers.h>
#include <podio/podioVersion.h>

template <typename BufferMap>
void releasePodioCollectionBuffers(BufferMap& buffers) {
#if podio_VERSION_MAJOR < 1
    for (auto& [_, buffer] : buffers) {
        if (buffer.deleteBuffers) {
            buffer.deleteBuffers(buffer);
        }
    }
#endif
    buffers.clear();
}
