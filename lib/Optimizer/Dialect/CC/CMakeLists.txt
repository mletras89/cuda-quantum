# ============================================================================ #
# Copyright (c) 2022 - 2025 NVIDIA Corporation & Affiliates.                   #
# All rights reserved.                                                         #
#                                                                              #
# This source code and the accompanying materials are made available under     #
# the terms of the Apache License 2.0 which accompanies this distribution.     #
# ============================================================================ #

add_cudaq_dialect_library(CCDialect
  CCDialect.cpp
  CCOps.cpp
  CCTypes.cpp

  DEPENDS
    CCDialectIncGen
    CCOpsIncGen
    CCTypesIncGen

  LINK_LIBS
    MLIRComplexDialect
    MLIRFuncDialect
    MLIRLLVMDialect
    MLIRIR
)
