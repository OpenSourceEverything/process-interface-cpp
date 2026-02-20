set(GPI_EXAMPLE_IPC_SOURCES "")
foreach(source_file IN LISTS GPI_COMMON_SOURCES GPI_IPC_SOURCES)
  list(APPEND GPI_EXAMPLE_IPC_SOURCES "${CMAKE_SOURCE_DIR}/${source_file}")
endforeach()

add_executable(
  gpi_example_talk_server
  talk_server.cpp
  ${GPI_EXAMPLE_IPC_SOURCES}
)
target_include_directories(
  gpi_example_talk_server
  PRIVATE
  ${CMAKE_SOURCE_DIR}/external
  ${CMAKE_SOURCE_DIR}/external/libzmq/include
)
target_compile_definitions(gpi_example_talk_server PRIVATE IPC_BACKEND_ZMQ=1)
target_link_libraries(gpi_example_talk_server PRIVATE ${GPI_ZMQ_TARGET})
set_target_properties(
  gpi_example_talk_server
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(
  gpi_example_talk_client
  talk_client.cpp
  ${GPI_EXAMPLE_IPC_SOURCES}
)
target_include_directories(
  gpi_example_talk_client
  PRIVATE
  ${CMAKE_SOURCE_DIR}/external
  ${CMAKE_SOURCE_DIR}/external/libzmq/include
)
target_compile_definitions(gpi_example_talk_client PRIVATE IPC_BACKEND_ZMQ=1)
target_link_libraries(gpi_example_talk_client PRIVATE ${GPI_ZMQ_TARGET})
set_target_properties(
  gpi_example_talk_client
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
