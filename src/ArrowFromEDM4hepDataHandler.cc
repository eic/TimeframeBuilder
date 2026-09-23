#ifdef HAVE_ARROW

#include "ArrowFromEDM4hepDataHandler.h"
#include "EDM4hepFrameBuilder.h"
#include <podio/utilities/ArrowFrameConverter.h>
#include <arrow/io/file.h>
#include <arrow/ipc/writer.h>
#include <arrow/table.h>
#include <arrow/record_batch.h>
#include <edm4hep/EventHeaderData.h>
#include <iostream>
#include <stdexcept>

ArrowFromEDM4hepDataHandler::~ArrowFromEDM4hepDataHandler()
{
    if (arrow_writer_) arrow_writer_->Close().ok();
    if (arrow_stream_) arrow_stream_->Close().ok();
}

void ArrowFromEDM4hepDataHandler::initializeOutput(
    const MergerConfig& config,
    const std::vector<std::unique_ptr<DataSource>>& data_sources)
{
    output_filename_ = config.output_file;
    // Discover collection names and initialize TChain sources.
    // No TTree or TFile needed — Arrow stream is opened lazily on first write.
    discoverCollections(data_sources);
    std::cout << "ArrowFromEDM4hepDataHandler output initialized: "
              << output_filename_ << std::endl;
}

void ArrowFromEDM4hepDataHandler::writeTimeframe()
{
    edm4hep::EventHeaderData header{};
    header.eventNumber = static_cast<uint64_t>(current_timeframe_number_);
    header.runNumber   = 0;
    header.timeStamp   = static_cast<uint64_t>(current_timeframe_number_);
    collections_.event_headers.push_back(header);

    auto frame = buildEDM4hepFrame(collections_, tracker_collection_names_,
                                   calo_collection_names_, gp_collection_names_);

    // Open output stream on first call.
    // /dev/null is a valid destination for throughput-only benchmarks on Linux.
    if (!arrow_stream_) {
        auto result = arrow::io::FileOutputStream::Open(output_filename_);
        if (!result.ok()) {
            throw std::runtime_error(
                "ArrowFromEDM4hepDataHandler: cannot open '" + output_filename_ +
                "': " + result.status().ToString());
        }
        arrow_stream_ = result.ValueOrDie();
    }

    auto colls = frame.getAvailableCollections();
    auto table = podio::convertFrameToTable(frame, colls);
    if (!table) throw std::runtime_error("ArrowFromEDM4hepDataHandler: convertFrameToTable nullptr");

    arrow::TableBatchReader reader(*table);
    std::shared_ptr<arrow::RecordBatch> batch;
    if (!reader.ReadNext(&batch).ok() || !batch) return;

    if (!arrow_writer_) {
        auto wr = arrow::ipc::MakeStreamWriter(arrow_stream_, batch->schema());
        if (!wr.ok()) throw std::runtime_error("ArrowFromEDM4hepDataHandler: MakeStreamWriter: " +
                                               wr.status().ToString());
        arrow_writer_ = wr.ValueOrDie();
    }
    auto status = arrow_writer_->WriteRecordBatch(*batch);
    if (!status.ok()) throw std::runtime_error("ArrowFromEDM4hepDataHandler: WriteRecordBatch: " +
                                               status.ToString());
    std::cout << "=== Timeframe written (Arrow/ROOT) ===" << std::endl;
}

void ArrowFromEDM4hepDataHandler::finalize()
{
    if (arrow_writer_) { arrow_writer_->Close().ok(); arrow_writer_.reset(); }
    if (arrow_stream_) { arrow_stream_->Close().ok(); arrow_stream_.reset(); }
    std::cout << "ArrowFromEDM4hepDataHandler output finalized: " << output_filename_ << std::endl;
}

#endif // HAVE_ARROW
