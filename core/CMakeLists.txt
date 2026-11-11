add_library(event_merger OBJECT
    src/CommandLineParser.cc
    src/DataHandler.cc
    src/DataSource.cc
    src/EventMerger.cc
    src/ParameterTransforms.cc
)

target_include_directories(event_merger
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(event_merger PRIVATE
    ${TIMEFRAME_BUILDER_YAML_CPP_TARGET}
)
