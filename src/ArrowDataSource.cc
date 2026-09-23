#ifdef HAVE_ARROW

#include "ArrowDataSource.h"
#include <podio/utilities/ArrowFrameConverter.h>
#include <arrow/io/file.h>
#include <arrow/table.h>
#include <arrow/ipc/reader.h>
#include <arrow/record_batch.h>
#include <iostream>

ArrowDataSource::ArrowDataSource(const SourceConfig& config, size_t source_index)
    : PodioEDM4hepDataSource(config, source_index) {
}

void ArrowDataSource::initialize(const std::vector<std::string>& tracker_collections,
                                 const std::vector<std::string>& calo_collections,
                                 const std::vector<std::string>& gp_collections) {
    // For Arrow sources, we discover collections from the Arrow schema
    // at stream-open time. Update inherited collection name pointers.
    tracker_collection_names_ = &tracker_collections;
    calo_collection_names_ = &calo_collections;
    gp_collection_names_ = &gp_collections;

    // Guard against double-initialization
    if (initialized_) return;

    // Open the stream and read the schema
    openStream();
}

void ArrowDataSource::openStream() {
    // Open input stream (regular file or FIFO)
    if (config_->input_files.empty()) {
        throw std::runtime_error("ArrowDataSource: no input files specified");
    }

    std::string input_path = config_->input_files[0];  // Single Arrow stream per source

    arrow::Result<std::shared_ptr<arrow::io::ReadableFile>> file_result;
    if (input_path == "-" || input_path == "/dev/stdin") {
        // stdin is handled as a readable file in Arrow
        file_result = arrow::io::ReadableFile::Open("/dev/stdin");
    } else {
        file_result = arrow::io::ReadableFile::Open(input_path);
    }

    if (!file_result.ok()) {
        throw std::runtime_error("ArrowDataSource: Failed to open Arrow input '" + input_path +
                                 "': " + file_result.status().ToString());
    }

    input_stream_ = file_result.ValueOrDie();

    // Test if stream is seekable
    auto seek_result = input_stream_->Seek(0);
    stream_is_seekable_ = seek_result.ok();
    if (stream_is_seekable_) {
        seek_result = input_stream_->Seek(0);  // Reset to start
        if (!seek_result.ok()) {
            stream_is_seekable_ = false;  // Should not happen if first seek succeeded
        }
    }

    // Create RecordBatchStreamReader
    auto reader_result = arrow::ipc::RecordBatchStreamReader::Open(input_stream_);
    if (!reader_result.ok()) {
        throw std::runtime_error("ArrowDataSource: Failed to open RecordBatchStreamReader: " +
                                 reader_result.status().ToString());
    }

    reader_ = reader_result.ValueOrDie();
    auto schema = reader_->schema();
    if (!schema) {
        throw std::runtime_error("ArrowDataSource: Failed to read Arrow schema");
    }

    std::cout << "Opened Arrow input stream from: " << config_->input_files[0] << std::endl;

    // If seekable, pre-scan to count batches
    if (stream_is_seekable_) {
        int64_t batch_count = 0;
        while (true) {
            auto batch_result = reader_->Next();
            if (!batch_result.ok()) {
                throw std::runtime_error("ArrowDataSource: Error reading batches: " +
                                         batch_result.status().ToString());
            }
            auto batch = batch_result.ValueOrDie();
            if (!batch) break;  // End of stream
            batch_count++;
        }
        total_entries_ = batch_count;

        // Re-open reader for actual reading
        auto seek_result = input_stream_->Seek(0);
        if (!seek_result.ok()) {
            throw std::runtime_error("ArrowDataSource: Failed to seek to beginning: " +
                                     seek_result.ToString());
        }
        reader_result = arrow::ipc::RecordBatchStreamReader::Open(input_stream_);
        if (!reader_result.ok()) {
            throw std::runtime_error("ArrowDataSource: Failed to re-open RecordBatchStreamReader: " +
                                     reader_result.status().ToString());
        }
        reader_ = reader_result.ValueOrDie();
        next_batch_index_ = 0;
        stream_exhausted_ = false;
    } else {
        // For pipes/FIFOs: unknown size
        total_entries_ = std::numeric_limits<size_t>::max();
        next_batch_index_ = 0;
        stream_exhausted_ = false;
    }

    initialized_ = true;
    std::cout << "Arrow source " << source_index_ << " initialized with " << total_entries_
              << " entries (seekable: " << (stream_is_seekable_ ? "yes" : "no") << ")" << std::endl;
}

bool ArrowDataSource::readNextBatch() {
    if (stream_exhausted_) {
        return false;
    }

    auto batch_result = reader_->Next();
    if (!batch_result.ok()) {
        throw std::runtime_error("ArrowDataSource: Error reading Arrow batch: " +
                                 batch_result.status().ToString());
    }

    auto batch = batch_result.ValueOrDie();
    if (!batch) {
        // End of stream
        stream_exhausted_ = true;
        return false;
    }

    // Convert RecordBatch to a 1-row Table
    auto table_result = arrow::Table::FromRecordBatches({batch});
    if (!table_result.ok()) {
        throw std::runtime_error("ArrowDataSource: Failed to create Arrow Table from RecordBatch: " +
                                 table_result.status().ToString());
    }

    auto table = table_result.ValueOrDie();

    // Convert to podio::Frame using podio's conversion function
    // convertTableToFrame expects 0-based row index
    try {
        auto frame = podio::convertTableToFrame(table, 0);
        storeFrame(std::move(frame));
        ++next_sequential_index_;
    } catch (const std::exception& e) {
        throw std::runtime_error("ArrowDataSource: Failed to convert Arrow Table to podio::Frame: " +
                                 std::string(e.what()));
    }

    next_batch_index_++;
    return true;
}

bool ArrowDataSource::loadNextEvent() {
    if (!readNextBatch()) {
        // If repeat_on_eof and seekable, restart
        if (config_->repeat_on_eof && stream_is_seekable_) {
            auto seek_result = input_stream_->Seek(0);
            if (!seek_result.ok()) {
                return false;
            }
            auto reader_result = arrow::ipc::RecordBatchStreamReader::Open(input_stream_);
            if (!reader_result.ok()) {
                return false;
            }
            reader_ = reader_result.ValueOrDie();
            next_batch_index_ = 0;
            stream_exhausted_ = false;
            return readNextBatch();
        }
        return false;
    }
    return true;
}

void ArrowDataSource::loadEvent(size_t event_index) {
    // Arrow sources use purely sequential reads. This method is called with
    // event_index = current_entry_index, which increments by 1 each time.
    // When repeat_on_eof wraps around, re-open the stream.

    size_t actual_index = (config_->repeat_on_eof && total_entries_ > 0)
                              ? event_index % total_entries_
                              : event_index;

    // Detect wrap-around: actual_index jumped backwards from the last batch we read
    if (config_->repeat_on_eof && total_entries_ > 0 && actual_index < next_batch_index_) {
        // Wrap around: re-open the stream
        if (!stream_is_seekable_) {
            throw std::runtime_error("ArrowDataSource: Cannot repeat non-seekable stream (pipe/FIFO)");
        }
        auto seek_result = input_stream_->Seek(0);
        if (!seek_result.ok()) {
            throw std::runtime_error("ArrowDataSource: Failed to seek to beginning");
        }
        auto reader_result = arrow::ipc::RecordBatchStreamReader::Open(input_stream_);
        if (!reader_result.ok()) {
            throw std::runtime_error("ArrowDataSource: Failed to re-open RecordBatchStreamReader");
        }
        reader_ = reader_result.ValueOrDie();
        next_batch_index_ = 0;
        stream_exhausted_ = false;
    }

    if (!readNextBatch()) {
        throw std::runtime_error("ArrowDataSource: Failed to read Arrow batch for event " +
                                 std::to_string(event_index));
    }
}

bool ArrowDataSource::hasMoreEntries() const {
    // Early return for repeat_on_eof: if enabled and we have entries, always return true
    if (config_->repeat_on_eof && total_entries_ > 0) {
        return true;
    }
    // For pipes: always true until ReadNext() returns null (checked in readNextBatch)
    // For seekable files: compare next_batch_index_ against total_entries_
    if (stream_is_seekable_) {
        return next_batch_index_ < static_cast<int64_t>(total_entries_);
    }
    return !stream_exhausted_;
}

#endif  // HAVE_ARROW
