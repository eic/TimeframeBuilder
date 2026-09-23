#pragma once

#ifdef HAVE_ARROW

#include "PodioEDM4hepDataSource.h"
#include <arrow/ipc/reader.h>
#include <arrow/io/interfaces.h>
#include <memory>

/**
 * @class ArrowDataSource
 * @brief EDM4hep data source reading from Arrow IPC streams.
 *
 * Reads Arrow IPC streams (files, FIFOs, pipes, stdin) sequentially.
 * Each RecordBatch is converted to a podio::Frame via podio::convertTableToFrame(),
 * then lazy-extraction logic inherited from PodioEDM4hepDataSource populates the
 * cached *Data struct vectors.
 *
 * Access is purely sequential (no random seeks). For repeat_on_eof, the stream
 * is re-opened from the beginning.
 */
class ArrowDataSource : public PodioEDM4hepDataSource {
public:
    ArrowDataSource(const SourceConfig& config, size_t source_index);
    ~ArrowDataSource() override = default;

    // DataSource interface
    void initialize(const std::vector<std::string>& tracker_collections,
                    const std::vector<std::string>& calo_collections,
                    const std::vector<std::string>& gp_collections) override;

    bool hasMoreEntries() const override;
    bool loadNextEvent() override;
    void loadEvent(size_t event_index) override;

    std::string getFormatName() const override { return "EDM4hep (Arrow IPC)"; }

private:
    // Open the Arrow stream reader and initialize schema/collection discovery
    void openStream();

    // Read the next RecordBatch and convert to podio::Frame
    bool readNextBatch();

    std::shared_ptr<arrow::io::ReadableFile> input_stream_;
    std::shared_ptr<arrow::ipc::RecordBatchStreamReader> reader_;
    int64_t next_batch_index_{0};
    bool stream_exhausted_{false};
    bool stream_is_seekable_{false};
};

#endif  // HAVE_ARROW
