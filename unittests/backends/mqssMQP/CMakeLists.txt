# ============================================================================ #
# Copyright (c) 2022 - 2024 NVIDIA Corporation & Affiliates.                   #
# All rights reserved.                                                         #
#                                                                              #
# This source code and the accompanying materials are made available under     #
# the terms of the Apache License 2.0 which accompanies this distribution.     #
# ============================================================================ #

add_executable(test-mqp-mqss MQSS-MQP-Tester.cpp)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT APPLE)
  target_link_options(test-mqp-mqss PRIVATE -Wl,--no-as-needed)
endif()
target_compile_definitions(test-mqp-mqss PRIVATE -DNVQIR_BACKEND_NAME=mqssMQP)
target_include_directories(test-mqp-mqss PRIVATE ../..)
target_link_libraries(test-mqp-mqss
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

configure_file("MQSS-MQP-StartServerAndTest.sh.in" "${CMAKE_BINARY_DIR}/unittests/backends/mqssMQP/MQSS-MQP-StartServerAndTest.sh" @ONLY)
add_test(NAME mqp-mqss-tests COMMAND bash MQSS-MQP-StartServerAndTest.sh WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/unittests/backends/mqssMQP/)

set(CMAKE_CXX_STANDARD 17)

### Add Crow headers
include_directories(${CMAKE_SOURCE_DIR}/tpls/Crow/include)

add_executable(CrowRestServer crow-REST-server.cpp)
## Link with uuid library
target_link_libraries(CrowRestServer PRIVATE
			uuid)

configure_file("CXX-MQSS-MQP-StartServerAndTest.sh.in" "${CMAKE_BINARY_DIR}/unittests/backends/mqssMQP/CXX-MQSS-MQP-StartServerAndTest.sh" @ONLY)
add_test(NAME mqp-mqss-cpp-tests COMMAND bash CXX-MQSS-MQP-StartServerAndTest.sh WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/unittests/backends/mqssMQP/)
