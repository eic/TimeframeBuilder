target_sources(timeframe_builder PRIVATE
    src/HepMC3DataHandler.cc
    src/HepMC3DataSource.cc
)
target_include_directories(timeframe_builder PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    ${ROOT_INCLUDE_DIRS}
)
target_include_directories(event_merger PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_compile_definitions(event_merger PRIVATE HAVE_HEPMC3)
target_link_libraries(timeframe_builder PRIVATE
    ${ROOT_LIBRARIES}
    HepMC3::HepMC3
    HepMC3::rootIO
)
target_link_libraries(event_merger PRIVATE
    ${ROOT_LIBRARIES}
    HepMC3::HepMC3
    HepMC3::rootIO
)

set(TIMEFRAME_BUILDER_HAVE_HEPMC3 ON PARENT_SCOPE)
message(STATUS "HepMC3 backend enabled")
