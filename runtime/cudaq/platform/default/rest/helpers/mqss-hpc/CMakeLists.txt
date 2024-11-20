# ============================================================================ #
# Copyright (c) 2022 - 2024 NVIDIA Corporation & Affiliates.                   #
# All rights reserved.                                                         #
#                                                                              #
# This source code and the accompanying materials are made available under     #
# the terms of the Apache License 2.0 which accompanies this distribution.     #
# ============================================================================ #
target_sources(cudaq-rest-qpu PRIVATE MQSS-HPC.cpp)
add_target_config(mqssHPC)

add_library(cudaq-serverhelper-mqss-hpc SHARED MQSS-HPC.cpp)
target_link_libraries(cudaq-serverhelper-mqss-hpc
  PUBLIC 
    cudaq-common 
    fmt::fmt-header-only
)
install(TARGETS cudaq-serverhelper-mqss-hpc DESTINATION lib)
