#pragma once

#ifdef HAVE_ARROW

#include "EDM4hepDataHandler.h"
#include <arrow/io/interfaces.h>
#include <arrow/ipc/api.h>
#include <memory>
#include <string>

/**
 * @class ArrowFromEDM4hepDataHandler
 * @brief ROOT TChain reader with Arrow IPC stream output.
 *
 * Inherits reading/merging from EDM4hepDataHandler, overrides output to write
 * Arrow IPC using the shared buildEDM4hepFrame() utility.
 *
 * Use /dev/null as the output path for throughput-only benchmarks:
 *   --output /dev/null --writer arrow
 */
class ArrowFromEDM4hepDataHandler : public EDM4hepDataHandler {
public:
    ArrowFromEDM4hepDataHandler() = default;
    ~ArrowFromEDM4hepDataHandler() override;

    void initializeOutput(const MergerConfig& config,
                          const std::vector<std::unique_ptr<DataSource>>& data_sources) override;

    void writeTimeframe() override;
    void finalize() override;

    std::string getFormatName() const override { return "EDM4hep→Arrow (ROOT reader)"; }

private:
    std::string output_filename_;
    std::shared_ptr<arrow::io::OutputStream>        arrow_stream_;
    std::shared_ptr<arrow::ipc::RecordBatchWriter>  arrow_writer_;
};

#endif // HAVE_ARROW
