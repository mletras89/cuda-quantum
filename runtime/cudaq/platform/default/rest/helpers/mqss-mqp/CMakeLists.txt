# ============================================================================ #
# Copyright (c) 2022 - 2024 NVIDIA Corporation & Affiliates.                   #
# All rights reserved.                                                         #
#                                                                              #
# This source code and the accompanying materials are made available under     #
# the terms of the Apache License 2.0 which accompanies this distribution.     #
# ============================================================================ #
target_sources(cudaq-rest-qpu PRIVATE MQSS-MQP-ServerHelper.cpp)
add_target_config(mqssMQP)

add_library(cudaq-serverhelper-mqss-mqp SHARED MQSS-MQP-ServerHelper.cpp)
target_link_libraries(cudaq-serverhelper-mqss-mqp
  PUBLIC 
    cudaq-common 
    fmt::fmt-header-only
)
install(TARGETS cudaq-serverhelper-mqss-mqp DESTINATION lib)

