# ============================================================================ #
# Copyright (c) 2022 - 2024 NVIDIA Corporation & Affiliates.                   #
# All rights reserved.                                                         #
#                                                                              #
# This source code and the accompanying materials are made available under     #
# the terms of the Apache License 2.0 which accompanies this distribution.     #
# ============================================================================ #

add_executable(test-hpc-mqss MQSS-HPC-Tester.cpp)
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT APPLE)
  target_link_options(test-hpc-mqss PRIVATE -Wl,--no-as-needed)
endif()
target_compile_definitions(test-hpc-mqss PRIVATE -DNVQIR_BACKEND_NAME=mqssHPC)
target_include_directories(test-hpc-mqss PRIVATE ../..)
target_link_libraries(test-hpc-mqss
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

configure_file("MQSS-HPC-StartServerAndTest.sh.in" "${CMAKE_BINARY_DIR}/unittests/backends/mqssHPC/MQSS-HPC-StartServerAndTest.sh" @ONLY)
add_test(NAME hpc-mqss-tests COMMAND bash MQSS-HPC-StartServerAndTest.sh WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/unittests/backends/mqssHPC/)

set(CMAKE_CXX_STANDARD 17)

### Add Crow headers
include_directories("/workspaces/cuda-quantum/tpls/Crow/include")

configure_file("CXX-MQSS-HPC-StartServerAndTest.sh.in" "${CMAKE_BINARY_DIR}/unittests/backends/mqssHPC/CXX-MQSS-HPC-StartServerAndTest.sh" @ONLY)
add_test(NAME hpc-mqss-cpp-tests COMMAND bash CXX-MQSS-HPC-StartServerAndTest.sh WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/unittests/backends/mqssHPC/)
