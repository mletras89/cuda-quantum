/* This code and any associated documentation is provided "as is"

Copyright 2025 Munich Quantum Software Stack Project

Licensed under the Apache License, Version 2.0 with LLVM Exceptions (the
"License"); you may not use this file except in compliance with the License.
You may obtain a copy of the License at

TODO

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
License for the specific language governing permissions and limitations under
the License.

SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
-------------------------------------------------------------------------
  author Martin Letras
  @date   November 2024
  @version 1.0
  @ brief
  
  Struct defining the information stored by a Quantum Task received by the MQSS.

*******************************************************************************
* This source code and the accompanying materials are made available under    *
* the terms of the Apache License 2.0 which accompanies this distribution.    *
******************************************************************************/
#pragma once


struct QuantumJob {
  std::string task_id = "";
  int n_qbits = 1;
  int n_shots = 1000;
  std::vector<std::string> circuit_files = {};
  std::string circuit_file_type = "quake";
  std::string result_destination = "";
  std::string preferred_qpu = "iqm";
  std::string scheduled_qpu = "";
  // QDMI_Device scheduled_qpu;
  int priority = 0;
  int optimisation_level = 0;
  bool no_modify = false;
  bool transpiler_flag = true;
  int result_type = 0;
  std::string submit_time = "";
  std::vector<std::string> circuits_qiskit = {};
  std::string additional_information = "";
  std::vector<std::string> restricted_resource_names = {};
  std::string user_identity = "";
  std::string token = "";
  bool via_hpc = false;
  // ThreadSafeModule thread_safe_module;
};

inline bool parseBool(const std::string& str) {
    std::string value = str;

    // Convert to lowercase
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);

    if (value == "true" || value == "1") return true;
    if (value == "false" || value == "0") return false;

    throw std::invalid_argument("Invalid boolean string: " + str);
}
