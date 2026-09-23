#ifdef HAVE_ARROW

#include "ArrowDataHandler.h"
#include <podio/utilities/ArrowFrameConverter.h>
#include <arrow/io/file.h>
#include <arrow/ipc/writer.h>
#include <arrow/table.h>
#include <arrow/record_batch.h>
#include <iostream>
#include <stdexcept>

// ---------------------------------------------------------------------------

ArrowDataHandler::~ArrowDataHandler()
{
    // Ensure resources are released even if finalize() was not called
    if (arrow_writer_) {
        auto status = arrow_writer_->Close();
        if (!status.ok()) {
            std::cerr << "ArrowDataHandler: writer Close() failed: " << status.ToString() << std::endl;
        }
    }
    if (arrow_stream_) {
        auto status = arrow_stream_->Close();
        if (!status.ok()) {
            std::cerr << "ArrowDataHandler: stream Close() failed: " << status.ToString() << std::endl;
        }
    }
}

void ArrowDataHandler::finalize()
{
    if (arrow_writer_) {
        auto status = arrow_writer_->Close();
        if (!status.ok()) {
            throw std::runtime_error("ArrowDataHandler: writer Close() failed: " + status.ToString());
        }
        arrow_writer_.reset();
    }
    if (arrow_stream_) {
        auto status = arrow_stream_->Close();
        if (!status.ok()) {
            throw std::runtime_error("ArrowDataHandler: stream Close() failed: " + status.ToString());
        }
        arrow_stream_.reset();
    }
    std::cout << "ArrowDataHandler output finalized: " << output_filename_ << std::endl;
}

void ArrowDataHandler::writeFrame(podio::Frame& frame)
{
    // Open the output stream on first write.
    // /dev/null is a valid destination on Linux for pure throughput benchmarks.
    if (!arrow_stream_) {
        auto result = arrow::io::FileOutputStream::Open(output_filename_);
        if (!result.ok()) {
            throw std::runtime_error(
                "ArrowDataHandler: cannot open output file '" + output_filename_ +
                "': " + result.status().ToString());
        }
        arrow_stream_ = result.ValueOrDie();
    }

    // Convert the frame to an Arrow Table
    auto colls = frame.getAvailableCollections();
    auto table = podio::convertFrameToTable(frame, colls);
    if (!table) {
        throw std::runtime_error("ArrowDataHandler: convertFrameToTable returned nullptr");
    }

    // Read row(s) from the table as RecordBatches
    arrow::TableBatchReader batch_reader(*table);
    std::shared_ptr<arrow::RecordBatch> batch;
    auto status = batch_reader.ReadNext(&batch);
    if (!status.ok()) {
        throw std::runtime_error("ArrowDataHandler: ReadNext failed: " + status.ToString());
    }
    if (!batch) {
        // Empty frame — nothing to write
        return;
    }

    // Create the IPC stream writer once (schema is fixed after the first frame)
    if (!arrow_writer_) {
        auto writer_result = arrow::ipc::MakeStreamWriter(arrow_stream_, batch->schema());
        if (!writer_result.ok()) {
            throw std::runtime_error(
                "ArrowDataHandler: MakeStreamWriter failed: " + writer_result.status().ToString());
        }
        arrow_writer_ = writer_result.ValueOrDie();
    }

    status = arrow_writer_->WriteRecordBatch(*batch);
    if (!status.ok()) {
        throw std::runtime_error("ArrowDataHandler: WriteRecordBatch failed: " + status.ToString());
    }
}

#endif // HAVE_ARROW
