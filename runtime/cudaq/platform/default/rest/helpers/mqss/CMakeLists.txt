# ============================================================================ #
# Copyright (c) 2022 - 2024 NVIDIA Corporation & Affiliates.                   #
# All rights reserved.                                                         #
#                                                                              #
# This source code and the accompanying materials are made available under     #
# the terms of the Apache License 2.0 which accompanies this distribution.     #
# ============================================================================ #
target_sources(cudaq-rest-qpu PRIVATE MQSSServerHelper.cpp)
add_target_config(mqss)

add_library(cudaq-serverhelper-mqss SHARED MQSSServerHelper.cpp)
target_link_libraries(cudaq-serverhelper-mqss
  PUBLIC 
    cudaq-common 
    fmt::fmt-header-only
)
install(TARGETS cudaq-serverhelper-mqss DESTINATION lib)

