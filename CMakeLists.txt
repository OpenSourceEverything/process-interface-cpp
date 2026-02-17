cmake_minimum_required(VERSION 3.15)
project(process_interface_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(
  GPI_STATUS_SOURCES
  status/api.cpp
  status/context.cpp
  status/debug.cpp
  status/engine.cpp
  status/error_map.cpp
  status/paths.cpp
  status/spec_loader.cpp
  status/writer.cpp
)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bridge_host/main.cpp" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bridge_adapter/bridge_adapter.cpp")
  add_executable(
    gpi_bridge_host
    bridge_host/main.cpp
    bridge_adapter/bridge_adapter.cpp
    ${GPI_STATUS_SOURCES}
    wire_v0/wire_v0.cpp
  )
  target_link_libraries(gpi_bridge_host PRIVATE Ws2_32)
endif()

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fixture_host/main.cpp" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/fixture_adapter/fixture_adapter.cpp")
  add_executable(
    gpi_fixture_host
    fixture_host/main.cpp
    fixture_adapter/fixture_adapter.cpp
    ${GPI_STATUS_SOURCES}
    wire_v0/wire_v0.cpp
  )
  target_link_libraries(gpi_fixture_host PRIVATE Ws2_32)
endif()
