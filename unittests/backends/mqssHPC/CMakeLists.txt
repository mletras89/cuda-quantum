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

# Add RabbitMQ
find_package(PkgConfig REQUIRED)
pkg_check_modules(RABBITMQ REQUIRED librabbitmq)
include_directories(${RABBITMQ_INCLUDE_DIRS})

add_executable(RabbitMQ-Consumer RabbitMQ-Consumer.cpp)
## Link with uuid library
target_link_libraries(RabbitMQ-Consumer PRIVATE
  cudaq-common
  cudaq
  cudaq-builder
  cudaq-mlir-runtime
  cudaq-rest-qpu
  cudaq-spin
  #nvqir nvqir-qppi
  ${RABBITMQ_LIBRARIES}
  cudaq-platform-default
  uuid)


configure_file("MQSS-HPC-StartServerAndTest.sh.in" "${CMAKE_BINARY_DIR}/unittests/backends/mqssHPC/MQSS-HPC-StartServerAndTest.sh" @ONLY)
configure_file("SimulateKernelLoweredToLLVM.py.in" "${CMAKE_BINARY_DIR}/unittests/backends/mqssHPC/SimulateKernelLoweredToLLVM.py" @ONLY)

add_test(NAME hpc-mqss-tests COMMAND bash MQSS-HPC-StartServerAndTest.sh WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/unittests/backends/mqssHPC/)
