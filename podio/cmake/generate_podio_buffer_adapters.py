#!/usr/bin/env python3
"""Generate typed podio buffer adapters from one or more podio datamodel YAMLs."""

import argparse
import re
from pathlib import Path

import yaml
from podio_gen.podio_config_reader import MemberParser


NUMERIC_TYPES = {
    "char",
    "signed char",
    "unsigned char",
    "short",
    "short int",
    "unsigned short",
    "unsigned short int",
    "int",
    "unsigned",
    "unsigned int",
    "long",
    "long int",
    "unsigned long",
    "unsigned long int",
    "long long",
    "long long int",
    "unsigned long long",
    "unsigned long long int",
    "float",
    "double",
    "int8_t",
    "uint8_t",
    "int16_t",
    "uint16_t",
    "int32_t",
    "uint32_t",
    "int64_t",
    "uint64_t",
    "std::int8_t",
    "std::uint8_t",
    "std::int16_t",
    "std::uint16_t",
    "std::int32_t",
    "std::uint32_t",
    "std::int64_t",
    "std::uint64_t",
}


def qualified(name):
    return "::" + name.replace("::", "::")


def collection_name(name):
    return name + "Collection"


def include_path(name, suffix):
    return name.replace("::", "/") + suffix


def parse_members(parser, definition, key):
    return [
        parser.parse(member, require_description=False)
        for member in definition.get(key, [])
    ]


def normalize_type(type_name):
    return re.sub(r"\s+", " ", type_name.strip())


def load_datatypes(yaml_paths):
    parser = MemberParser()
    datatypes = {}
    for yaml_path in yaml_paths:
        with open(yaml_path, "r", encoding="utf-8") as model_file:
            model = yaml.safe_load(model_file)
        if not isinstance(model, dict) or not isinstance(
            model.get("datatypes"), dict
        ):
            raise ValueError(f"{yaml_path} does not define a datatypes mapping")
        components = {
            name: parse_members(parser, definition, "Members")
            for name, definition in model.get("components", {}).items()
        }

        for name, definition in model["datatypes"].items():
            if name in datatypes:
                raise ValueError(
                    f"Datatype {name} appears in more than one model YAML"
                )
            members = parse_members(parser, definition, "Members")
            vertex_member = next(
                (
                    member
                    for member in members
                    if member.name == "vertex"
                    and member.full_type in components
                    and all(
                        component_member.name in {"x", "y", "z"}
                        and normalize_type(component_member.full_type)
                        in NUMERIC_TYPES
                        for component_member in components[member.full_type]
                        if component_member.name in {"x", "y", "z"}
                    )
                    and {"x", "y", "z"}.issubset(
                        {
                            component_member.name
                            for component_member in components[member.full_type]
                        }
                    )
                ),
                None,
            )
            status_member = next(
                (
                    member
                    for member in members
                    if member.name == "generatorStatus"
                    and member.is_builtin
                    and not member.is_array
                    and normalize_type(member.full_type) in NUMERIC_TYPES
                ),
                None,
            )
            event_header_fields = {
                member.name: member
                for member in members
                if member.is_builtin
                and not member.is_array
                and normalize_type(member.full_type) in NUMERIC_TYPES
            }
            datatypes[name] = {
                "header": include_path(name, "Data.h"),
                "data_type": qualified(name) + "Data",
                "collection_type": collection_name(name),
                "members": members,
                "components": components,
                "beam_vertex": (
                    (status_member.name, vertex_member.name)
                    if status_member and vertex_member
                    else None
                ),
                "event_header": (
                    tuple(
                        event_header_fields[field]
                        for field in ("eventNumber", "runNumber", "timeStamp", "weight")
                    )
                    if all(
                        field in event_header_fields
                        for field in ("eventNumber", "runNumber", "timeStamp", "weight")
                    )
                    and not (
                        parse_members(parser, definition, "OneToManyRelations")
                        or parse_members(parser, definition, "OneToOneRelations")
                    )
                    else None
                ),
                "relations": (
                    parse_members(parser, definition, "OneToManyRelations")
                    + parse_members(parser, definition, "OneToOneRelations")
                ),
                "many_relations": parse_members(
                    parser, definition, "OneToManyRelations"
                ),
                "vector_members": parse_members(parser, definition, "VectorMembers"),
            }
    return datatypes


def generate(yaml_paths):
    datatypes = load_datatypes(yaml_paths)
    headers = {"PodioBufferAdapterSupport.h"}
    for definition in datatypes.values():
        headers.add(definition["header"])
        for member in definition["vector_members"]:
            if not member.is_builtin:
                if member.full_type.startswith("std::array<"):
                    headers.add("array")
                elif member.namespace and member.namespace != "std":
                    headers.add(
                        include_path(
                            f"{member.namespace}::{member.bare_type}", ".h"
                        )
                    )
                else:
                    headers.add(include_path(member.full_type, ".h"))

    lines = [
        '#include "PodioBufferAdapterSupport.h"',
        "",
        "#include <stdexcept>",
        "#include <string>",
        "#include <vector>",
    ]
    lines.extend(f"#include <{header}>" for header in sorted(headers - {"PodioBufferAdapterSupport.h"}))
    lines.extend(
        [
            "",
            "void registerGeneratedPodioAdapters() {",
            "    static const bool registered = [] {",
            "        auto& transforms = RecordTransformRegistry::instance();",
        ]
    )

    for datatype, definition in sorted(datatypes.items()):
        for member in definition["members"]:
            if (
                member.is_builtin
                and not member.is_array
                and normalize_type(member.full_type) in NUMERIC_TYPES
            ):
                lines.append(
                    "        transforms.registerNumericField<"
                    f"{definition['data_type']}>("
                    f'"{datatype}", "{member.name}", '
                    f"&{definition["data_type"]}::{member.name});"
                )
            elif member.full_type in definition["components"]:
                for component_member in definition["components"][
                    member.full_type
                ]:
                    if (
                        component_member.is_builtin
                        and not component_member.is_array
                        and normalize_type(component_member.full_type)
                        in NUMERIC_TYPES
                    ):
                        lines.extend(
                            [
                                "        transforms.registerFieldAccessor("
                                f'"{datatype}", "{member.name}.{component_member.name}",',
                                f"            [](void* record) {{",
                                f"                auto* data = static_cast<{definition['data_type']}*>(record);",
                                "                return NumericFieldAccessor{",
                                "                    [data] { return static_cast<double>("
                                f"data->{member.name}.{component_member.name}); }},",
                                "                    [data](double value) {",
                                f"                        data->{member.name}.{component_member.name} = "
                                "checkedNumericCast<decltype("
                                f"data->{member.name}.{component_member.name})>(value);",
                                "                    }};",
                                "            });",
                            ]
                        )

    for datatype, definition in sorted(datatypes.items()):
        relations = definition["relations"]
        many_relations = definition["many_relations"]
        vector_members = definition["vector_members"]
        relation_count = len(relations)
        lines.extend(
            [
                "        registerPodioBufferAdapter<"
                f"{definition['data_type']}>("
                f'"{definition["collection_type"]}", "{datatype}", '
                f"{relation_count},",
                "            [](auto& data, const auto& relation_offsets,",
                "               const auto& member_offsets) {",
                f"                if (relation_offsets.size() != {relation_count} ||",
                f"                    member_offsets.size() != {len(vector_members)}) {{",
                "                    throw std::runtime_error("
                '"Generated adapter layout mismatch");',
                "                }",
            ]
        )
        for index, relation in enumerate(many_relations):
            lines.extend(
                [
                    f'                addPodioRangeOffset(data.{relation.name}_begin, '
                    f"relation_offsets[{index}], \"{datatype}\");",
                    f'                addPodioRangeOffset(data.{relation.name}_end, '
                    f"relation_offsets[{index}], \"{datatype}\");",
                ]
            )
        for index, member in enumerate(vector_members):
            lines.extend(
                [
                    f'                addPodioRangeOffset(data.{member.name}_begin, '
                    f"member_offsets[{index}], \"{datatype}\");",
                    f'                addPodioRangeOffset(data.{member.name}_end, '
                    f"member_offsets[{index}], \"{datatype}\");",
                ]
            )
        lines.append("            },")
        if vector_members:
            lines.append("            {")
            for index, member in enumerate(vector_members):
                lines.append(
                    f'                podioVectorMemberAdapter<{member.full_type}>('
                    f'{index}, "{member.full_type}"),'
                )
            lines.append("            });")
        else:
            lines.append("            {});")
        if definition["beam_vertex"]:
            status_member, vertex_member = definition["beam_vertex"]
            lines.extend(
                [
                    "        registerPodioBeamVertexAdapter<"
                    f"{definition['data_type']}>("
                    f'"{definition["collection_type"]}",',
                    "            [](const auto& values) -> "
                    "std::optional<PodioBufferMerger::BeamVertex> {",
                    "                for (const auto& value : values) {",
                    f"                    if (value.{status_member} == 1) {{",
                    "                        return "
                    "PodioBufferMerger::BeamVertex{",
                    f"                            static_cast<float>(value.{vertex_member}.x),",
                    f"                            static_cast<float>(value.{vertex_member}.y),",
                    f"                            static_cast<float>(value.{vertex_member}.z)}};",
                    "                    }",
                    "                }",
                    "                return std::nullopt;",
                    "            });",
                ]
            )
        if definition["event_header"]:
            event_number, run_number, time_stamp, weight = definition["event_header"]
            lines.extend(
                [
                    "        registerPodioEventHeaderAdapter("
                    f'"{definition["collection_type"]}",',
                    "            [](podio::CollectionReadBuffers& buffers, const "
                    "PodioBufferMerger::EventHeaderValues& values) {",
                    "                auto* data = "
                    f"buffers.dataAsVector<{definition['data_type']}>();",
                    "                if (!data || !buffers.vectorMembers ||",
                    f"                    buffers.vectorMembers->size() != {len(vector_members)}) {{",
                    "                    throw std::runtime_error("
                    '"Event-header buffer layout mismatch");',
                    "                }",
                    f"                {definition['data_type']} header{{}};",
                    f"                header.{event_number.name} = "
                    f"static_cast<decltype(header.{event_number.name})>(values.event_number);",
                    f"                header.{run_number.name} = "
                    f"static_cast<decltype(header.{run_number.name})>(values.run_number);",
                    f"                header.{time_stamp.name} = "
                    f"static_cast<decltype(header.{time_stamp.name})>(values.time_stamp);",
                    f"                header.{weight.name} = "
                    f"static_cast<decltype(header.{weight.name})>(values.weight);",
                ]
            )
            for index, member in enumerate(vector_members):
                lines.extend(
                    [
                        f"                auto vector_member_{index} = "
                        f"podioVectorMemberAdapter<{member.full_type}>("
                        f"{index}, \"{member.full_type}\");",
                        f"                const auto vector_offset_{index} = "
                        f"vector_member_{index}.size(buffers);",
                        f"                addPodioRangeOffset(header.{member.name}_begin, "
                        f"vector_offset_{index}, \"{datatype}\");",
                        f"                addPodioRangeOffset(header.{member.name}_end, "
                        f"vector_offset_{index}, \"{datatype}\");",
                    ]
                )
            lines.extend(
                [
                    "                data->push_back(header);",
                    "            });",
                ]
            )

    lines.extend(["        return true;", "    }();", "    (void)registered;", "}"])
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--yaml", required=True, nargs="+", type=Path)
    arguments = parser.parse_args()

    generated = generate(arguments.yaml)
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text(generated, encoding="utf-8")


if __name__ == "__main__":
    main()
