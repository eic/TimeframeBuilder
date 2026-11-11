set(TIMEFRAME_BUILDER_DATAMODEL_YAMLS "" CACHE STRING
    "Semicolon-separated podio datamodel YAML files used to generate buffer adapters")
set(TIMEFRAME_BUILDER_DATAMODEL_LIBRARIES "" CACHE STRING
    "Semicolon-separated podio datamodel libraries linked into timeframe_builder")
set(TIMEFRAME_BUILDER_DATAMODEL_PACKAGES "" CACHE STRING
    "Semicolon-separated CMake packages providing additional podio datamodel targets")

foreach(model_package IN LISTS TIMEFRAME_BUILDER_DATAMODEL_PACKAGES)
    find_package(${model_package} REQUIRED)
endforeach()

if(NOT TIMEFRAME_BUILDER_DATAMODEL_YAMLS)
    if(EDM4HEP_DATA_DIR AND EXISTS "${EDM4HEP_DATA_DIR}/edm4hep.yaml")
        set(TIMEFRAME_BUILDER_DATAMODEL_YAMLS
            "${EDM4HEP_DATA_DIR}/edm4hep.yaml")
    else()
        find_file(_EDM4HEP_DATAMODEL_YAML edm4hep.yaml
            HINTS "$ENV{EDM4HEP_DATA_DIR}"
            PATH_SUFFIXES share/edm4hep
            PATHS /usr/local/share/edm4hep /usr/share/edm4hep)
        if(_EDM4HEP_DATAMODEL_YAML)
            set(TIMEFRAME_BUILDER_DATAMODEL_YAMLS
                "${_EDM4HEP_DATAMODEL_YAML}")
        endif()
    endif()
endif()

if(NOT TIMEFRAME_BUILDER_DATAMODEL_YAMLS)
    message(STATUS
        "Podio backend disabled: no datamodel YAML found. Set "
        "TIMEFRAME_BUILDER_DATAMODEL_YAMLS to enable it.")
    return()
endif()

set(PODIO_ADAPTERS_SOURCE
    "${CMAKE_CURRENT_BINARY_DIR}/generated/PodioDataModelAdapters.cc")
add_custom_command(
    OUTPUT "${PODIO_ADAPTERS_SOURCE}"
    COMMAND ${CMAKE_COMMAND} -E env
        --unset=PYTHONHOME
        "PYTHONPATH=${podio_PYTHON_DIR}"
        ${Python3_EXECUTABLE}
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/generate_podio_buffer_adapters.py"
        --output "${PODIO_ADAPTERS_SOURCE}"
        --yaml ${TIMEFRAME_BUILDER_DATAMODEL_YAMLS}
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/generate_podio_buffer_adapters.py"
        ${TIMEFRAME_BUILDER_DATAMODEL_YAMLS}
    VERBATIM
)
add_custom_target(podio_generate_adapters DEPENDS "${PODIO_ADAPTERS_SOURCE}")

target_sources(timeframe_builder PRIVATE
    src/PodioBufferAdapterSupport.cc
    src/PodioBufferMerger.cc
    src/PodioDataHandler.cc
    src/PodioDataSource.cc
    src/PodioFrameBuilder.cc
)
target_include_directories(timeframe_builder PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    ${ROOT_INCLUDE_DIRS}
)
target_include_directories(event_merger PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_definitions(event_merger PRIVATE HAVE_PODIO)
target_link_libraries(timeframe_builder PRIVATE
    ${ROOT_LIBRARIES}
    podio::podioRootIO
    ${TIMEFRAME_BUILDER_DATAMODEL_LIBRARIES}
)
target_link_libraries(event_merger PRIVATE
    ${ROOT_LIBRARIES}
    podio::podioRootIO
)

if(TARGET EDM4HEP::edm4hep)
    target_link_libraries(timeframe_builder PRIVATE EDM4HEP::edm4hep)
endif()
if(TARGET EDM4HEP::edm4hepDict)
    target_link_libraries(timeframe_builder PRIVATE EDM4HEP::edm4hepDict)
endif()

set(TIMEFRAME_BUILDER_HAVE_PODIO ON PARENT_SCOPE)
set(TIMEFRAME_BUILDER_PODIO_ADAPTERS_SOURCE
    "${PODIO_ADAPTERS_SOURCE}" PARENT_SCOPE)
message(STATUS "Podio backend enabled")
