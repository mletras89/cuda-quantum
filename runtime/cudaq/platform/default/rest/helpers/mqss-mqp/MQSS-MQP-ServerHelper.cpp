/* This code and any associated documentation is provided "as is"

Copyright 2025 Munich Quantum Software Stack Project

Licensed under the Apache License, Version 2.0 with LLVM Exceptions (the
"License"); you may not use this file except in compliance with the License.
You may obtain a copy of the License at

https://github.com/Munich-Quantum-Software-Stack/MQSS-CUDAQ-Adapter/tree/develop?tab=readme-ov-file#

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
  
  Server helper used to connect CUDAQ runtime to the Munich Quantum Software 
  Stack (MQSS). The communication is done via REST api to reach the Munich
  Quantum Portal (MQP).

*******************************************************************************
* This source code and the accompanying materials are made available under    *
* the terms of the Apache License 2.0 which accompanies this distribution.    *
******************************************************************************/
#include "cudaq.h"
#include "common/Logger.h"
#include "common/ServerHelper.h"
#include "cudaq/utils/cudaq_utils.h"
#include "common/RestClient.h"
#include "common/QuantumTask.h"

#include <fstream>
#include <thread>
#include "MQSSJobStatus.h"

namespace cudaq {

std::map<std::string, std::string> readKeyValueFileToMap(const std::string& filename);
std::string trim(const std::string& str); 

/// @brief The MQSSServerHelper implements the ServerHelper interface
/// to map Job requests and Job result retrievals actions from the calling
/// Executor to the specific schema required by the remote MQSS REST
/// server.
class MQSSServerHelper : public ServerHelper {
protected:
  /// @brief The base URL
  std::string mqpUrl = "https://portal.quantum.lrz.de:4000/v1/";
  std::string token = ""; // read at runtime, from env varible MQSS_MQP_TOKEN
  // information read from the configuration that has to be passed to the MQSS
  QuantumJob quantumTask;
  /// @brief Return the headers required for the REST calls
  RestHeaders generateRequestHeader() const;

public:
  /// @brief Return the name of this server helper, must be the
  /// same as the qpu config file.
  const std::string name() const override { return "mqssMQP"; }
  RestHeaders getHeaders() override;

  void initialize(BackendConfig config) override {
    backendConfig = config;
    // reading information from the configuration
    auto envConfiguration = getenv("MQSS_CONFIGURATION");
    if (envConfiguration) {
      std::map<std::string, std::string> customConfiguration = readKeyValueFileToMap(envConfiguration);

      auto iter = customConfiguration.find("transpiler_flag");
      if (iter != customConfiguration.end())
        quantumTask.transpiler_flag = parseBool(iter->second);
      iter = customConfiguration.find("preferred_qpu");
      if (iter != customConfiguration.end())
        quantumTask.preferred_qpu = iter->second;
      iter = customConfiguration.find("restricted_resource_names");
      if (iter != customConfiguration.end())
        quantumTask.restricted_resource_names = parseStringList(iter->second);
      iter = customConfiguration.find("priority");
      if (iter != customConfiguration.end())
        quantumTask.priority = std::stoi(iter->second);
      iter = customConfiguration.find("optimisation_level");
      if (iter != customConfiguration.end())
        quantumTask.optimisation_level = std::stoi(iter->second);
    }
    // let the token to be read from an env variable    
    token = loadTokenFromEnvFile();
   // Allow overriding MQSS Server Url, the compiled program will still work if
    // architecture matches. This is useful in case we're using the same program
    // against different backends, for example simulated and actually connected
    // to the hardware.
    auto envMQSSServerUrl = getenv("MQSS_MQP_SERVER_URL");
    if (envMQSSServerUrl) 
      mqpUrl = std::string(envMQSSServerUrl);
    if (!mqpUrl.ends_with("/"))
      mqpUrl += "/";
    parseConfigForCommonParams(config);
  }

  std::string loadTokenFromEnvFile() {
    const char* envPath = std::getenv("MQSS_MQP_TOKEN");
    if (!envPath) {
        throw std::runtime_error("Environment variable MQSS_MQP_TOKEN is not set.");
    }
    std::ifstream file(envPath);
    if (!file) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
  }

  /// @brief Create a job payload for the provided quantum codes
  ServerJobPayload
  createJob(std::vector<KernelExecution> &circuitCodes) override;

  /// @brief Return the job id from the previous job post
  std::string extractJobId(ServerMessage &postResponse) override;

  /// @brief Return the URL for retrieving job status
  std::string constructGetJobPath(ServerMessage &postResponse) override;
  std::string constructGetJobPath(std::string &jobId) override;

  /// @brief Return true if the job is done
  bool jobIsDone(ServerMessage &getJobResponse) override;

  /// @brief Given a completed job response, map back to the sample_result
  cudaq::sample_result processResults(ServerMessage &postJobResponse,
                                      std::string &jobID) override;
};

ServerJobPayload
MQSSServerHelper::createJob(std::vector<KernelExecution> &circuitCodes) {
  std::vector<ServerMessage> messages;
  for (auto &circuitCode : circuitCodes) {
    // Construct the job itself
    ServerMessage j;
    // assigning circuit files object
    j["circuit"] = circuitCode.code;
    j["circuit_format"] = "qasm"; // submitting quake to mqss
    j["resource_name"] = quantumTask.preferred_qpu;
    j["shots"] = shots;//quantumTask.n_shots;  
    j["no_modify"]  = quantumTask.no_modify;
    j["queued"]  = false;
    messages.push_back(j);
  }
  // Get the headers
  RestHeaders headers = generateRequestHeader();
  // Return the payloadi
  return std::make_tuple(mqpUrl + "job", headers, messages);
}

std::string MQSSServerHelper::extractJobId(ServerMessage &postResponse) {
  return postResponse["uuid"].get<std::string>();
}

std::string
MQSSServerHelper::constructGetJobPath(ServerMessage &postResponse) {
  // In order to work with the MQSS via MQP, the GetJobPath is used to get 
  // the status of a job
  return mqpUrl + "job/" + extractJobId(postResponse)+"/status";
}

std::string MQSSServerHelper::constructGetJobPath(std::string &jobId) {
  // In order to work with the MQSS via MQP, the GetJobPath is used to get 
  // the status of a job
  return mqpUrl + "job/" + jobId+"/status";
}

bool MQSSServerHelper::jobIsDone(ServerMessage &getJobResponse) {
  auto status = getJobResponse["status"].get<std::string>();
  if (status == cudaq::mqss::jobStatusToString(cudaq::mqss::JobStatus::FAILED) || 
      status == cudaq::mqss::jobStatusToString(cudaq::mqss::JobStatus::CANCELLED))
    throw std::runtime_error("MQSS::MQP job failed to execute!");

  return status == cudaq::mqss::jobStatusToString(cudaq::mqss::JobStatus::COMPLETED);
}

cudaq::sample_result
MQSSServerHelper::processResults(ServerMessage &postJobResponse,
                                       std::string &jobId) {
  // MQP has a separeted request for asking results, this only will work if it is
  // fired after we are 100% sure, the job is completed
// Get job result
  RestClient client;
  auto headers = generateRequestHeader();
  auto resultResponse = client.get(mqpUrl + "job/" + jobId + "/result", "", headers);

  // Extract and parse the "result" field (it's a JSON-encoded string)
  if (!resultResponse.contains("result"))
    throw std::runtime_error("Missing 'result' field in response");

  // Parse inner JSON string into object
  std::string resultString = resultResponse["result"];
  nlohmann::json parsedCounts = nlohmann::json::parse(resultString);

  cudaq::info("Parsed aggregated results: {}", parsedCounts.dump());

  // Convert to CountsDictionary
  cudaq::CountsDictionary counts;
  for (auto &[bitstring, count] : parsedCounts.items()) {
    counts[bitstring] = count.get<std::size_t>();
  }

  // Reconstruct sequentialData vector
  std::vector<std::string> sequentialData;
  for (auto &[bitstring, count] : counts) {
    for (size_t i = 0; i < count; ++i) {
      sequentialData.push_back(bitstring);
    }
  }

  // Sort for consistency (optional)
  std::sort(sequentialData.begin(), sequentialData.end());

  // Create global result
  std::vector<ExecutionResult> srs;
  srs.emplace_back(counts, GlobalRegisterName);
  srs.back().sequentialData = sequentialData;

  return sample_result(srs);
}

std::map<std::string, std::string>
MQSSServerHelper::generateRequestHeader() const {
  std::map<std::string, std::string> headers{
      {"Authorization", "Bearer "+token},
       {"Content-Type", "application/json"}};
  return headers;
}

RestHeaders MQSSServerHelper::getHeaders() {
  return generateRequestHeader();
}

/// Trim leading and trailing whitespace (helper)
std::string trim(const std::string& str) {
  const std::string whitespace = " \t\n\r";
  const auto begin = str.find_first_not_of(whitespace);
  if (begin == std::string::npos) return "";
  const auto end = str.find_last_not_of(whitespace);
  return str.substr(begin, end - begin + 1);
}

/// Read file and parse key:value lines into a map
std::map<std::string, std::string> readKeyValueFileToMap(const std::string& filename) {
  std::ifstream file(filename);
  if (!file.is_open())
    throw std::runtime_error("Could not open file: " + filename);

  std::map<std::string, std::string> result;
  std::string line;
  int lineNumber = 0;

  while (std::getline(file, line)) {
    lineNumber++;
    if (line.empty()) continue;

    size_t colonPos = line.find(':');
    if (colonPos == std::string::npos)
      throw std::runtime_error("Invalid line " + std::to_string(lineNumber) +
                               ": no ':' found");

    std::string key = trim(line.substr(0, colonPos));
    std::string value = trim(line.substr(colonPos + 1));

    if (key.empty() || value.empty())
      throw std::runtime_error("Invalid key or value at line " + std::to_string(lineNumber));

    result[key] = value;
  }

  return result;
}
} // namespace cudaq
CUDAQ_REGISTER_TYPE(cudaq::ServerHelper, cudaq::MQSSServerHelper,
                    mqssMQP)
