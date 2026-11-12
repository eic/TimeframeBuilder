#pragma once

#include "EDM4hepDataHandler.h"
#include <podio/Frame.h>
#include <vector>
#include <string>

/**
 * @brief Convert an EDM4hepMergedCollections struct into a podio::Frame.
 *
 * Shared by PodioROOTDataHandler and any Arrow-output handler regardless of
 * which reader was used upstream.  The caller supplies the collection-name
 * vectors so the function has no dependency on handler internals.
 *
 * The returned Frame owns typed edm4hep collections reconstructed from the
 * raw *Data structs, with MCParticle parent/daughter relations, tracker-hit
 * particle links, and calo-hit → contribution → particle chains fully
 * restored.
 */
podio::Frame buildEDM4hepFrame(
    const EDM4hepMergedCollections&     collections,
    const std::vector<std::string>&     tracker_collection_names,
    const std::vector<std::string>&     calo_collection_names,
    const std::vector<std::string>&     gp_collection_names);
