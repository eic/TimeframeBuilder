#include "ParameterTransforms.h"

#include "DataSource.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace {

std::string requiredParameter(
    const std::unordered_map<std::string, std::string>& parameters,
    const std::string& name, const std::string& plugin) {
    const auto parameter = parameters.find(name);
    if (parameter == parameters.end() || parameter->second.empty()) {
        throw std::invalid_argument("Plugin '" + plugin +
                                    "' requires parameter '" + name + "'");
    }
    return parameter->second;
}

double numericParameter(
    const std::unordered_map<std::string, std::string>& parameters,
    const std::string& name, double default_value) {
    const auto parameter = parameters.find(name);
    if (parameter == parameters.end()) {
        return default_value;
    }

    size_t parsed = 0;
    const auto value = std::stod(parameter->second, &parsed);
    if (parsed != parameter->second.size()) {
        throw std::invalid_argument("Plugin parameter '" + name +
                                    "' must be numeric");
    }
    return value;
}

bool matches(const std::string& selector, const std::string& value) {
    return selector == "all" || selector == value;
}

} // namespace

double EventTransformContext::number(const std::string& name) const {
    const auto value = values.find(name);
    if (value == values.end()) {
        throw std::runtime_error("No event transformation value named '" + name +
                                 "'");
    }
    return value->second;
}

void EventTransformContext::setNumber(std::string name, double value) {
    if (name.empty()) {
        throw std::invalid_argument(
            "Event transformation values require a name");
    }
    values.insert_or_assign(std::move(name), value);
}

TransformRecordView::TransformRecordView(
    std::string datatype, std::string collection,
    std::unordered_map<std::string, NumericFieldAccessor> fields) :
    datatype_(std::move(datatype)),
    collection_(std::move(collection)),
    fields_(std::move(fields)) {
}

const std::string& TransformRecordView::datatype() const {
    return datatype_;
}

const std::string& TransformRecordView::collection() const {
    return collection_;
}

bool TransformRecordView::hasField(const std::string& field) const {
    return fields_.contains(field);
}

double TransformRecordView::read(const std::string& field) const {
    const auto accessor = fields_.find(field);
    if (accessor == fields_.end() || !accessor->second.read) {
        throw std::runtime_error("Datatype '" + datatype_ +
                                 "' has no readable numeric field '" + field +
                                 "'");
    }
    return accessor->second.read();
}

void TransformRecordView::write(const std::string& field, double value) const {
    const auto accessor = fields_.find(field);
    if (accessor == fields_.end() || !accessor->second.write) {
        throw std::runtime_error("Datatype '" + datatype_ +
                                 "' has no writable numeric field '" + field +
                                 "'");
    }
    accessor->second.write(value);
}

EventTransformRegistry& EventTransformRegistry::instance() {
    static EventTransformRegistry registry;
    return registry;
}

EventTransformRegistry::EventTransformRegistry() {
    registerPlugin(
        "source_time_offset",
        [](EventTransformContext& context, const EventTransformInput& input,
           const EventTransformRule&) {
            const auto& config = input.source_config;
            if (config.already_merged) {
                context.setNumber("time_offset", 0.0);
                return;
            }

            std::uniform_real_distribution<double> uniform(
                0.0, input.timeframe_duration);
            auto time_offset = uniform(input.random);
            if (config.use_bunch_crossing) {
                if (input.bunch_crossing_period <= 0.0f) {
                    throw std::invalid_argument(
                        "Bunch crossing period must be positive");
                }
                time_offset =
                    std::floor(time_offset / input.bunch_crossing_period) *
                    input.bunch_crossing_period;
            }

            if (config.attach_to_beam) {
                if (config.beam_speed == 0.0f) {
                    throw std::invalid_argument("Beam speed must be non-zero");
                }
                const auto vertex = input.source.beamVertexPosition();
                const auto distance =
                    vertex.z * std::cos(config.beam_angle) +
                    vertex.x * std::sin(config.beam_angle);
                time_offset += distance / config.beam_speed;

                if (config.beam_spread > 0.0f) {
                    std::normal_distribution<double> spread(
                        0.0, config.beam_spread);
                    time_offset += spread(input.random);
                }
            }

            context.setNumber("time_offset", time_offset);
        });
}

void EventTransformRegistry::registerPlugin(std::string name, Plugin plugin) {
    if (name.empty() || !plugin) {
        throw std::invalid_argument(
            "Event transformation plugins require a name and implementation");
    }
    if (!plugins_.emplace(std::move(name), std::move(plugin)).second) {
        throw std::invalid_argument("Duplicate event transformation plugin");
    }
}

void EventTransformRegistry::validateRules(
    const std::vector<EventTransformRule>& rules) const {
    for (const auto& rule : rules) {
        if (!plugins_.contains(rule.plugin)) {
            throw std::runtime_error(
                "Unknown event transformation plugin: " + rule.plugin);
        }
        if (rule.sources.empty()) {
            throw std::runtime_error(
                "Event transformation plugin '" + rule.plugin +
                "' must target at least one source or 'all'");
        }
    }
}

EventTransformContext EventTransformRegistry::prepare(
    const DataSource& source, const SourceConfig& source_config,
    float timeframe_duration, float bunch_crossing_period,
    std::mt19937& random,
    const std::vector<EventTransformRule>& rules) const {
    EventTransformContext context;
    context.setNumber(
        "generator_status_offset",
        source_config.already_merged
            ? 0.0
            : static_cast<double>(source_config.generator_status_offset));
    context.setNumber("time_offset", 0.0);

    const EventTransformInput input{source, source_config, timeframe_duration,
                                    bunch_crossing_period, random};
    for (const auto& rule : rules) {
        if (std::find(rule.sources.begin(), rule.sources.end(), "all") ==
                rule.sources.end() &&
            std::find(rule.sources.begin(), rule.sources.end(),
                      source_config.name) == rule.sources.end()) {
            continue;
        }
        const auto plugin = plugins_.find(rule.plugin);
        if (plugin == plugins_.end()) {
            throw std::runtime_error(
                "Unknown event transformation plugin: " + rule.plugin);
        }
        plugin->second(context, input, rule);
    }
    return context;
}

RecordTransformRegistry& RecordTransformRegistry::instance() {
    static RecordTransformRegistry registry;
    return registry;
}

RecordTransformRegistry::RecordTransformRegistry() {
    registerPlugin(
        "add_context_value",
        [](TransformRecordView& record, const RecordTransformRule& rule,
           const EventTransformContext& context) {
            const auto context_name = requiredParameter(
                rule.parameters, "context", rule.plugin);
            const auto scale = numericParameter(rule.parameters, "scale", 1.0);
            record.write(rule.field,
                         record.read(rule.field) +
                             context.number(context_name) * scale);
        });
    registerPlugin(
        "add_field_value",
        [](TransformRecordView& record, const RecordTransformRule& rule,
           const EventTransformContext&) {
            const auto source_field = requiredParameter(
                rule.parameters, "source_field", rule.plugin);
            const auto scale =
                numericParameter(rule.parameters, "scale", 1.0);
            const auto offset =
                numericParameter(rule.parameters, "offset", 0.0);
            record.write(rule.field,
                         record.read(rule.field) +
                             record.read(source_field) * scale + offset);
        });
}

void RecordTransformRegistry::registerPlugin(std::string name,
                                             Plugin plugin) {
    if (name.empty() || !plugin) {
        throw std::invalid_argument(
            "Record transformation plugins require a name and implementation");
    }
    if (!plugins_.emplace(std::move(name), std::move(plugin)).second) {
        throw std::invalid_argument("Duplicate record transformation plugin");
    }
}

void RecordTransformRegistry::registerFieldAccessor(
    const std::string& datatype, const std::string& field,
    FieldAccessorFactory accessor) {
    if (datatype.empty() || field.empty() || !accessor) {
        throw std::invalid_argument(
            "Record field accessors require a datatype, field, and accessor");
    }
    if (!field_accessors_[datatype]
             .emplace(field, std::move(accessor))
             .second) {
        throw std::invalid_argument("Duplicate record field accessor for " +
                                    datatype + "." + field);
    }
}

void RecordTransformRegistry::validateRules(
    const std::vector<RecordTransformRule>& rules) const {
    for (const auto& rule : rules) {
        if (!plugins_.contains(rule.plugin)) {
            throw std::runtime_error(
                "Unknown record transformation plugin: " + rule.plugin);
        }
        if (rule.datatype.empty() || rule.collection.empty() ||
            rule.field.empty()) {
            throw std::runtime_error(
                "Record transformation targets require datatype, collection, "
                "and field selectors");
        }
        if (rule.datatype != "all") {
            const auto datatype = field_accessors_.find(rule.datatype);
            if (datatype == field_accessors_.end() ||
                !datatype->second.contains(rule.field)) {
                throw std::runtime_error(
                    "No field accessor for configured target " + rule.datatype +
                    "." + rule.field);
            }
        }
    }
}

void RecordTransformRegistry::apply(
    const std::string& datatype, const std::string& collection, void* record,
    const std::vector<RecordTransformRule>& rules,
    const EventTransformContext& context) const {
    std::vector<const RecordTransformRule*> matching_rules;
    for (const auto& rule : rules) {
        if (matches(rule.datatype, datatype) &&
            matches(rule.collection, collection)) {
            matching_rules.push_back(&rule);
        }
    }
    if (matching_rules.empty()) {
        return;
    }

    const auto datatype_accessors = field_accessors_.find(datatype);
    if (datatype_accessors == field_accessors_.end()) {
        throw std::runtime_error("No record field accessors registered for '" +
                                 datatype + "'");
    }

    std::unordered_map<std::string, NumericFieldAccessor> fields;
    fields.reserve(datatype_accessors->second.size());
    for (const auto& [field, accessor] : datatype_accessors->second) {
        fields.emplace(field, accessor(record));
    }
    TransformRecordView view{datatype, collection, std::move(fields)};

    for (const auto* rule : matching_rules) {
        if (!view.hasField(rule->field)) {
            if (rule->datatype == "all") {
                continue;
            }
            throw std::runtime_error(
                "Configured field '" + rule->field + "' is not available in " +
                datatype + " collection '" + collection + "'");
        }
        const auto plugin = plugins_.find(rule->plugin);
        if (plugin == plugins_.end()) {
            throw std::runtime_error(
                "Unknown record transformation plugin: " + rule->plugin);
        }
        plugin->second(view, *rule, context);
    }
}
