#ifdef HAVE_ARROW

#include "ArrowInputDataHandler.h"
#include "ArrowDataSource.h"

std::vector<std::unique_ptr<DataSource>> ArrowInputDataHandler::initializeDataSources(
    const std::string& filename,
    const std::vector<SourceConfig>& source_configs) {
    std::vector<std::unique_ptr<DataSource>> sources;

    for (size_t i = 0; i < source_configs.size(); ++i) {
        auto source = std::make_unique<ArrowDataSource>(source_configs[i], i);
        sources.push_back(std::move(source));
    }

    return sources;
}

#endif  // HAVE_ARROW
