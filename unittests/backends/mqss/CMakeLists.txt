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

configure_file("MQSSStartServerAndTest.sh.in" "${CMAKE_BINARY_DIR}/unittests/backends/mqss/MQSSStartServerAndTest.sh" @ONLY)
add_test(NAME mqss-tests COMMAND bash MQSSStartServerAndTest.sh WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/unittests/backends/MQSS/)
