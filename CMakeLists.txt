cmake_minimum_required(VERSION 3.16)

include(cmake/git_version.cmake)
set_git_version(TIMEFRAME_BUILDER_VERSION)

project(TimeframeBuilder VERSION ${TIMEFRAME_BUILDER_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(TIMEFRAME_BUILDER_ENABLE_PODIO
    "Build the Podio data-format backend when its dependencies are available" ON)
option(TIMEFRAME_BUILDER_ENABLE_HEPMC3
    "Build the HepMC3 data-format backend when its dependencies are available" ON)

add_compile_definitions(
    TIMEFRAME_BUILDER_VERSION_STR="${TIMEFRAME_BUILDER_VERSION_FULL}")

find_package(yaml-cpp REQUIRED)
if(TARGET yaml-cpp::yaml-cpp)
    set(TIMEFRAME_BUILDER_YAML_CPP_TARGET yaml-cpp::yaml-cpp)
elseif(TARGET yaml-cpp)
    set(TIMEFRAME_BUILDER_YAML_CPP_TARGET yaml-cpp)
else()
    message(FATAL_ERROR
        "yaml-cpp was found, but its CMake package exports no usable target")
endif()

add_subdirectory(core)

add_executable(timeframe_builder core/src/timeframe_builder_main.cc)
target_link_libraries(timeframe_builder PRIVATE
    event_merger
    ${TIMEFRAME_BUILDER_YAML_CPP_TARGET}
)

set(TIMEFRAME_BUILDER_HAVE_PODIO OFF)
set(TIMEFRAME_BUILDER_HAVE_HEPMC3 OFF)

if(TIMEFRAME_BUILDER_ENABLE_PODIO OR TIMEFRAME_BUILDER_ENABLE_HEPMC3)
    find_package(ROOT QUIET)
endif()

if(TIMEFRAME_BUILDER_ENABLE_PODIO)
    if(ROOT_FOUND)
        find_package(Python3 QUIET COMPONENTS Interpreter)
        find_package(podio QUIET)

        if(podio_FOUND AND Python3_Interpreter_FOUND AND
           TARGET podio::podioRootIO)
            find_package(EDM4HEP QUIET)
            add_subdirectory(podio)
            if(TIMEFRAME_BUILDER_HAVE_PODIO)
                set_source_files_properties(
                    "${TIMEFRAME_BUILDER_PODIO_ADAPTERS_SOURCE}"
                    PROPERTIES GENERATED TRUE
                )
                target_sources(timeframe_builder PRIVATE
                    "${TIMEFRAME_BUILDER_PODIO_ADAPTERS_SOURCE}")
                add_dependencies(timeframe_builder podio_generate_adapters)
            endif()
        else()
            message(STATUS
                "Podio backend disabled: Podio, Python3, ROOT, or podio::podioRootIO was not found")
        endif()
    else()
        message(STATUS "Podio backend disabled: ROOT was not found")
    endif()
endif()

if(TIMEFRAME_BUILDER_ENABLE_HEPMC3)
    if(ROOT_FOUND)
        find_package(HepMC3 QUIET)
        if(HepMC3_FOUND AND TARGET HepMC3::rootIO)
            add_subdirectory(hepmc3)
        else()
            message(STATUS
                "HepMC3 backend disabled: HepMC3 with ROOT I/O was not found")
        endif()
    else()
        message(STATUS "HepMC3 backend disabled: ROOT was not found")
    endif()
endif()

if(NOT TIMEFRAME_BUILDER_HAVE_PODIO AND
   NOT TIMEFRAME_BUILDER_HAVE_HEPMC3)
    message(WARNING
        "No data-format backends were enabled; only core functionality will build")
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/install" CACHE PATH
        "Installation directory" FORCE)
endif()

install(TARGETS timeframe_builder DESTINATION bin)

include(CTest)
if(BUILD_TESTING)
    add_executable(data_source_offset_test
        tests/DataSourceOffsetTest.cc
        core/src/DataSource.cc
        core/src/ParameterTransforms.cc
    )
    target_include_directories(data_source_offset_test PRIVATE core/include)
    add_test(NAME data_source_offset COMMAND data_source_offset_test)

    add_executable(transformation_config_test
        tests/TransformationConfigTest.cc
        core/src/CommandLineParser.cc
    )
    target_include_directories(transformation_config_test PRIVATE core/include)
    target_compile_definitions(transformation_config_test PRIVATE
        TIMEFRAME_BUILDER_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(transformation_config_test PRIVATE
        ${TIMEFRAME_BUILDER_YAML_CPP_TARGET}
    )
    add_test(NAME transformation_config COMMAND transformation_config_test)
endif()
