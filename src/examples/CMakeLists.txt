set(GPI_EXAMPLE_IPC_SOURCES "")
foreach(source_file IN LISTS GPI_COMMON_SOURCES GPI_IPC_SOURCES)
  list(APPEND GPI_EXAMPLE_IPC_SOURCES "${CMAKE_SOURCE_DIR}/${source_file}")
endforeach()

function(gpi_add_example_ipc_target target_name)
  add_executable(
    ${target_name}
    ${ARGN}
    ${GPI_EXAMPLE_IPC_SOURCES}
  )
  target_include_directories(
    ${target_name}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/external
    ${CMAKE_SOURCE_DIR}/external/libzmq/include
  )
  target_compile_definitions(${target_name} PRIVATE IPC_BACKEND_ZMQ=1)
  target_link_libraries(${target_name} PRIVATE ${GPI_ZMQ_TARGET})
  set_target_properties(
    ${target_name}
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
  )
endfunction()

add_subdirectory(example1)
