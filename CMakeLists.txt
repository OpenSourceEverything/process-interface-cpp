cmake_minimum_required(VERSION 3.15)
project(process_interface_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(
  gpi_bridge_host
  bridge_host/main.cpp
  bridge_adapter/bridge_adapter.cpp
  wire_v0/wire_v0.cpp
)
