#pragma once

#ifdef HAVE_ARROW

#include "PodioROOTDataHandler.h"
#include <podio/Frame.h>
#include <memory>

// Forward-declare Arrow types to keep the header lightweight
namespace arrow {
class Schema;
namespace io   { class OutputStream; }
namespace ipc  { class RecordBatchWriter; }
} // namespace arrow

/**
 * @class ArrowDataHandler
 * @brief PodioROOTDataHandler subclass that writes the output as an Arrow IPC stream.
 *
 * Overrides writeFrame() to convert the podio::Frame to an Arrow Table via
 * podio::convertFrameToTable(), then writes each row as a RecordBatch into
 * an Arrow IPC stream file.
 *
 * Gate entire class with HAVE_ARROW so the build is clean when Arrow is absent.
 */
class ArrowDataHandler : public PodioROOTDataHandler {
public:
    ArrowDataHandler() = default;
    ~ArrowDataHandler() override;

    std::string getFormatName() const override { return "EDM4hep (Arrow IPC)"; }

    void finalize() override;

protected:
    void writeFrame(podio::Frame& frame) override;

private:
    std::shared_ptr<arrow::io::OutputStream>       arrow_stream_;
    std::shared_ptr<arrow::ipc::RecordBatchWriter> arrow_writer_;
};

#endif // HAVE_ARROW
