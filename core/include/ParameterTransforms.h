#pragma once

#include "MergerConfig.h"

#include <functional>
#include <cmath>
#include <limits>
#include <random>
#include <string>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <vector>

class DataSource;

struct EventTransformContext {
    std::unordered_map<std::string, double> values;

    double number(const std::string& name) const;
    void setNumber(std::string name, double value);
};

struct EventTransformInput {
    const DataSource& source;
    const SourceConfig& source_config;
    float timeframe_duration;
    float bunch_crossing_period;
    std::mt19937& random;
};

struct NumericFieldAccessor {
    std::function<double()> read;
    std::function<void(double)> write;
};

class TransformRecordView {
public:
    TransformRecordView(std::string datatype, std::string collection,
                        std::unordered_map<std::string, NumericFieldAccessor>
                            fields);

    const std::string& datatype() const;
    const std::string& collection() const;
    bool hasField(const std::string& field) const;
    double read(const std::string& field) const;
    void write(const std::string& field, double value) const;

private:
    std::string datatype_;
    std::string collection_;
    std::unordered_map<std::string, NumericFieldAccessor> fields_;
};

template <typename Value>
Value checkedNumericCast(double value) {
    static_assert(std::is_arithmetic_v<Value>);
    if (!std::isfinite(value) ||
        static_cast<long double>(value) <
            static_cast<long double>(std::numeric_limits<Value>::lowest()) ||
        static_cast<long double>(value) >
            static_cast<long double>(std::numeric_limits<Value>::max())) {
        throw std::overflow_error(
            "Record transformation value is outside the field's numeric range");
    }
    return static_cast<Value>(value);
}

class EventTransformRegistry {
public:
    using Plugin = std::function<void(
        EventTransformContext&, const EventTransformInput&,
        const EventTransformRule&)>;

    static EventTransformRegistry& instance();

    void registerPlugin(std::string name, Plugin plugin);
    void validateRules(const std::vector<EventTransformRule>& rules) const;
    EventTransformContext prepare(
        const DataSource& source, const SourceConfig& source_config,
        float timeframe_duration, float bunch_crossing_period,
        std::mt19937& random,
        const std::vector<EventTransformRule>& rules) const;

private:
    EventTransformRegistry();

    std::unordered_map<std::string, Plugin> plugins_;
};

class RecordTransformRegistry {
public:
    using FieldAccessorFactory =
        std::function<NumericFieldAccessor(void*)>;
    using Plugin = std::function<void(
        TransformRecordView&, const RecordTransformRule&,
        const EventTransformContext&)>;

    static RecordTransformRegistry& instance();

    void registerPlugin(std::string name, Plugin plugin);
    void registerFieldAccessor(const std::string& datatype,
                               const std::string& field,
                               FieldAccessorFactory accessor);

    template <typename Data, typename Value>
    void registerNumericField(const std::string& datatype,
                              const std::string& field,
                              Value Data::*member) {
        static_assert(std::is_arithmetic_v<Value>,
                      "Record transformations require arithmetic fields");

        registerFieldAccessor(
            datatype, field,
            [member](void* record) {
                auto* data = static_cast<Data*>(record);
                return NumericFieldAccessor{
                    [data, member] {
                        return static_cast<double>(data->*member);
                    },
                    [data, member](double value) {
                        data->*member = checkedNumericCast<Value>(value);
                    }};
            });
    }

    void validateRules(const std::vector<RecordTransformRule>& rules) const;
    void apply(const std::string& datatype, const std::string& collection,
               void* record, const std::vector<RecordTransformRule>& rules,
               const EventTransformContext& context) const;

private:
    RecordTransformRegistry();

    std::unordered_map<std::string, Plugin> plugins_;
    std::unordered_map<
        std::string,
        std::unordered_map<std::string, FieldAccessorFactory>>
        field_accessors_;
};
