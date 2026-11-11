#include "DataSource.h"
#include "ParameterTransforms.h"

#include <random>
#include <limits>
#include <unordered_map>
#include <vector>

namespace {

struct TestRecord {
    double target = 2.0;
    double source = 3.0;
    int bounded = 0;
};

class TestDataSource final : public DataSource {
public:
    explicit TestDataSource(const SourceConfig& config) {
        config_ = &config;
    }

    bool hasMoreEntries() const override { return false; }
    bool loadNextEvent() override { return false; }
    void loadEvent(size_t) override {}
    void printStatus() const override {}
    bool isInitialized() const override { return true; }
    std::string getFormatName() const override { return "test"; }

    mutable size_t vertex_reads{0};

protected:
    VertexPosition getBeamVertexPosition() const override {
        ++vertex_reads;
        return {0.0f, 0.0f, 299.792458f};
    }
};

} // namespace

int main() {
    SourceConfig config;
    config.attach_to_beam = true;

    TestDataSource source{config};
    std::mt19937 random{1234};

    const std::vector<EventTransformRule> disabled_rules{
        EventTransformRule{"source_time_offset", {"other"}, {}}};
    const auto disabled_context = EventTransformRegistry::instance().prepare(
        source, config, 1000.0f, 10.0f, random, disabled_rules);
    if (source.vertex_reads != 0 ||
        disabled_context.number("time_offset") != 0.0) {
        return 1;
    }

    const std::vector<EventTransformRule> rules{
        EventTransformRule{"source_time_offset", {"all"}, {}}};
    const auto context = EventTransformRegistry::instance().prepare(
        source, config, 1000.0f, 10.0f, random, rules);
    if (source.vertex_reads != 1 || context.number("time_offset") <= 0.0) {
        return 2;
    }

    auto& record_transforms = RecordTransformRegistry::instance();
    record_transforms.registerNumericField<TestRecord>(
        "TestRecord", "target", &TestRecord::target);
    record_transforms.registerNumericField<TestRecord>(
        "TestRecord", "source", &TestRecord::source);
    record_transforms.registerNumericField<TestRecord>(
        "TestRecord", "bounded", &TestRecord::bounded);
    const std::vector<RecordTransformRule> record_rules{
        RecordTransformRule{"add_field_value", "TestRecord", "Records",
                            "target",
                            {{"source_field", "source"},
                             {"scale", "2"},
                             {"offset", "1"}}}};
    record_transforms.validateRules(record_rules);
    TestRecord record;
    record_transforms.apply("TestRecord", "Records", &record, record_rules,
                            context);
    if (record.target != 9.0) {
        return 3;
    }

    auto overflow_context = context;
    overflow_context.setNumber("too_large", std::numeric_limits<double>::max());
    const std::vector<RecordTransformRule> overflow_rules{
        RecordTransformRule{"add_context_value", "TestRecord", "Records",
                            "bounded", {{"context", "too_large"}, {"scale", "2"}}}};
    try {
        record_transforms.apply("TestRecord", "Records", &record,
                                overflow_rules, overflow_context);
    } catch (const std::overflow_error&) {
        return 0;
    }
    return 4;
}
