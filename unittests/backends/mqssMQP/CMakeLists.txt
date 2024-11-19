# ============================================================================ #
# Copyright (c) 2022 - 2024 NVIDIA Corporation & Affiliates.                   #
# All rights reserved.                                                         #
#                                                                              #
# This source code and the accompanying materials are made available under     #
# the terms of the Apache License 2.0 which accompanies this distribution.     #
# ============================================================================ #

add_executable(test_mqss MQSSTester.cpp)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT APPLE)
  target_link_options(test_mqss PRIVATE -Wl,--no-as-needed)
endif()
target_compile_definitions(test_mqss PRIVATE -DNVQIR_BACKEND_NAME=mqss)
target_include_directories(test_mqss PRIVATE ../..)
target_link_libraries(test_mqss
  PRIVATE fmt::fmt-header-only 
  cudaq-common 
  cudaq
  cudaq-builder
  cudaq-mlir-runtime
  cudaq-rest-qpu
  cudaq-spin 
  nvqir nvqir-qpp
  cudaq-platform-default 
  gtest_main)

configure_file("MQSSStartServerAndTest.sh.in" "${CMAKE_BINARY_DIR}/unittests/backends/mqssMQP/MQSSStartServerAndTest.sh" @ONLY)
add_test(NAME mqss-tests COMMAND bash MQSSStartServerAndTest.sh WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/unittests/backends/MQSS/)


set(CMAKE_CXX_STANDARD 17)

### Add Crow headers
include_directories("/workspaces/cuda-quantum/tpls/Crow/include")

add_executable(CrowRestServer crow-REST-server.cpp)
## Link with uuid library
target_link_libraries(CrowRestServer PRIVATE
			uuid)

configure_file("CPPMQSSStartServerAndTest.sh.in" "${CMAKE_BINARY_DIR}/unittests/backends/mqss/CPPMQSSStartServerAndTest.sh" @ONLY)
add_test(NAME mqss-cpp-tests COMMAND bash CPPMQSSStartServerAndTest.sh WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/unittests/backends/MQSS/)
