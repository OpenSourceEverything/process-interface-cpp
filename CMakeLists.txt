cmake_minimum_required(VERSION 3.15)
project(process_interface_cpp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC ON CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(WITH_PERF_TOOL OFF CACHE BOOL "" FORCE)
set(WITH_DOC OFF CACHE BOOL "" FORCE)
set(WITH_DOCS OFF CACHE BOOL "" FORCE)
set(ENABLE_DRAFTS OFF CACHE BOOL "" FORCE)
set(ENABLE_CURVE OFF CACHE BOOL "" FORCE)
set(WITH_LIBSODIUM OFF CACHE BOOL "" FORCE)
set(WITH_TLS OFF CACHE BOOL "" FORCE)
set(WITH_NSS OFF CACHE BOOL "" FORCE)
set(ENABLE_WS OFF CACHE BOOL "" FORCE)
set(ZMQ_HAVE_IPC OFF CACHE BOOL "" FORCE)
set(ZMQ_HAVE_STRUCT_SOCKADDR_UN OFF CACHE BOOL "" FORCE)

add_subdirectory(external/libzmq EXCLUDE_FROM_ALL)

set(
  GPI_COMMON_SOURCES
  src/common/file_io.cpp
  src/common/path_templates.cpp
  src/common/text.cpp
  src/common/time_utils.cpp
)

set(
  GPI_IPC_SOURCES
  src/ipc/factory/IpcFactory.cpp
  src/ipc/impl/ZmqContext.cpp
  src/ipc/impl/ZmqIpcClient.cpp
  src/ipc/impl/ZmqIpcServer.cpp
)

set(
  GPI_PROCESS_INTERFACE_SOURCES
  src/process_interface/common/action_catalog.cpp
  src/process_interface/common/action_executor.cpp
  src/process_interface/common/action_jobs.cpp
  src/process_interface/common/action_response.cpp
  src/process_interface/common/control_script_runner.cpp
  src/process_interface/host/dispatcher.cpp
)

set(
  GPI_STATUS_SOURCES
  src/status/api.cpp
  src/status/debug.cpp
  src/status/error_map.cpp
  src/status/paths.cpp
  src/status/probes.cpp
  src/status/spec_loader.cpp
  src/status/status_engine.cpp
  src/status/status_expression_parser.cpp
  src/status/status_operation_registry.cpp
  src/status/writer.cpp
)

set(
  GPI_PLATFORM_SOURCES
  src/platform/file_replace.cpp
  src/platform/port_probe.cpp
  src/platform/process_exec.cpp
  src/platform/process_probe.cpp
)

set(
  GPI_RUNTIME_SOURCES
  src/host_runtime/host_profile.cpp
  src/host_runtime/host_runtime.cpp
  src/wire_v0/wire_v0.cpp
)

set(
  GPI_SHARED_SOURCES
  ${GPI_COMMON_SOURCES}
  ${GPI_IPC_SOURCES}
  ${GPI_PROCESS_INTERFACE_SOURCES}
  ${GPI_STATUS_SOURCES}
  ${GPI_PLATFORM_SOURCES}
  ${GPI_RUNTIME_SOURCES}
)

set(GPI_ZMQ_TARGET "")
if (TARGET libzmq-static)
  set(GPI_ZMQ_TARGET libzmq-static)
elseif(TARGET libzmq)
  set(GPI_ZMQ_TARGET libzmq)
else()
  message(FATAL_ERROR "libzmq target not available")
endif()

add_executable(
  gpi_host
  src/host/main.cpp
  ${GPI_SHARED_SOURCES}
)
target_include_directories(
  gpi_host
  PRIVATE
  external
  external/libzmq/include
)
target_compile_definitions(gpi_host PRIVATE IPC_BACKEND_ZMQ=1)
target_link_libraries(gpi_host PRIVATE ${GPI_ZMQ_TARGET})

add_executable(
  gpi_client
  src/client/main.cpp
  ${GPI_COMMON_SOURCES}
  ${GPI_IPC_SOURCES}
)
target_include_directories(
  gpi_client
  PRIVATE
  external
  external/libzmq/include
)
target_compile_definitions(gpi_client PRIVATE IPC_BACKEND_ZMQ=1)
target_link_libraries(gpi_client PRIVATE ${GPI_ZMQ_TARGET})

add_subdirectory(src/examples)
