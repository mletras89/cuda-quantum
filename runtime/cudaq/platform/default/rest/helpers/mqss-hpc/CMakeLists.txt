# This code and any associated documentation is provided "as is"
#
# Copyright 2025 Munich Quantum Software Stack Project
#
# Licensed under the Apache License, Version 2.0 with LLVM Exceptions (the
# "License"); you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# TODO
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
#
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
# *******************************************************************************
# * This source code and the accompanying materials are made available under
# * the terms of the Apache License 2.0 which accompanies this distribution.
# *******************************************************************************
target_sources(cudaq-rest-qpu PRIVATE MQSS-HPC.cpp)
add_target_config(mqssHPC)

add_library(cudaq-serverhelper-mqss-hpc SHARED MQSS-HPC.cpp)
target_link_libraries(cudaq-serverhelper-mqss-hpc
  PUBLIC 
    cudaq-common 
    fmt::fmt-header-only
)
install(TARGETS cudaq-serverhelper-mqss-hpc DESTINATION lib)
