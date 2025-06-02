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
#include <iostream>
#include <thread>
#include "MQSSJobStatus.h"

namespace cudaq {

/// @brief Find and set the API and refresh tokens, and the time string.
void findMQSSApiKeyInFile(std::string &apiKey, const std::string &path,
                      std::string &refreshKey, std::string &timeStr);

/// Search for the API key, invokes findMQSSApiKeyInFile
std::string searchMQSSAPIKey(std::string &key, std::string &refreshKey,
                         std::string &timeStr,
                         std::string userSpecifiedConfig = "");

/// @brief The MQSSServerHelper implements the ServerHelper interface
/// to map Job requests and Job result retrievals actions from the calling
/// Executor to the specific schema required by the remote MQSS REST
/// server.
class MQSSServerHelper : public ServerHelper {
protected:
  /// @brief The base URL
  std::string mqpUrl = "https://portal.quantum.lrz.de:4000/";
  std::string token = "6XHDN1U8WIxourN4lGP8OEuu3zDRI5Y1LoxJN6dAT7iftnpBlZ8tZ9pybZ05ipkh";
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
    // Set an alternate base URL if provided
    auto iter = backendConfig.find("url");
    if (iter != backendConfig.end()) {
      mqpUrl = iter->second;
      if (!mqpUrl.ends_with("/"))
        mqpUrl += "/";
    }
    // reading informatin from the configuration
    iter = backendConfig.find("n_qbits");
    if (iter != backendConfig.end())
      quantumTask.n_qbits = std::stoi(iter->second);
    iter = backendConfig.find("n_shots");
    if (iter != backendConfig.end())
      quantumTask.n_shots = std::stoi(iter->second);
    iter = backendConfig.find("preferred_qpu");
    if (iter != backendConfig.end())
      quantumTask.preferred_qpu = iter->second;
    iter = backendConfig.find("priority");
    if (iter != backendConfig.end())
      quantumTask.priority = std::stoi(iter->second);
    iter = backendConfig.find("optimisation_level");
    if (iter != backendConfig.end())
      quantumTask.optimisation_level = std::stoi(iter->second);
    iter = backendConfig.find("no_modify");
    if (iter != backendConfig.end())
      quantumTask.no_modify = parseBool(iter->second);
    iter = backendConfig.find("transpiler_flag");
    if (iter != backendConfig.end())
      quantumTask.transpiler_flag = parseBool(iter->second);
    iter = backendConfig.find("result_type");
    if (iter != backendConfig.end())
      quantumTask.result_type = std::stoi(iter->second);
    iter = backendConfig.find("additional_information");
    if (iter != backendConfig.end())
      quantumTask.additional_information = iter->second;
    iter = backendConfig.find("user_identity");
    if (iter != backendConfig.end())
      quantumTask.user_identity = iter->second;
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
    j["name"] = circuitCode.name;
    j["task_id"] = "", // mqss has to assign id
    j["n_qbits"] = quantumTask.n_qbits;
    j["n_shots"] = quantumTask.n_shots;
    // assigning circuit files object
    std::vector<std::string> circuit_files;
    circuit_files.push_back(circuitCode.code);
    j["circuit_files"] = circuit_files;
    j["circuit_file_type"] = "quake"; // submitting quake to mqss
    j["preferred_qpu"] = quantumTask.preferred_qpu;
    j["scheduled_qpu"] = "";  // mqss has to assign it
    j["result_destination"] = quantumTask.result_destination;
    j["priority"] = quantumTask.priority;
    j["optimisation_level"] = quantumTask.optimisation_level;
    j["no_modify"]  = quantumTask.no_modify;
    j["transpiler_flag"] = quantumTask.transpiler_flag;
    j["result_type"] = quantumTask.result_type;
    j["circuits_qiskit"]= nlohmann::json::array();
    j["additional_information"] = quantumTask.additional_information;
    j["restricted_resource_names"] = quantumTask.restricted_resource_names;
    j["user_identity"] = quantumTask.user_identity;
    j["token"] =  quantumTask.token;
    j["via_hpc"] = false; // via MQP
    // Get the current time as a time_point
    auto now = std::chrono::system_clock::now();
    // Convert time_point to time_t (which holds time in seconds)
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    // Format the time as a string
    std::ostringstream timeStream;
    timeStream << std::put_time(std::localtime(&currentTime), "%Y-%m-%d %H:%M:%S");

    j["submit_time"] = timeStream.str();  
    messages.push_back(j);
  }
  // Get the headers
  RestHeaders headers = generateRequestHeader();
  // Return the payload
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
  if (status == cudaq::mqss::jobStatusToString(cudaq::mqss::JobStatus::FAILED))
    throw std::runtime_error("MQSS::MQP job failed to execute!");

  return status == cudaq::mqss::jobStatusToString(cudaq::mqss::JobStatus::COMPLETED);
}

cudaq::sample_result
MQSSServerHelper::processResults(ServerMessage &postJobResponse,
                                       std::string &jobId) {
  // MQP has a separeted request for asking results, this only will work if it is
  // fired after we are 100% sure, the job is completed
  RestClient client;
  auto headers = generateRequestHeader();
  auto resultResponse = client.get(mqpUrl + "job/" +jobId+"/result", "", headers);
  // Results come back as a map of vectors. Each map key corresponds to a qubit
  // and its corresponding vector holds the measurement results in each shot:
  //      { "results" : { "r0" : ["0", "0", ...],
  //                      "r1" : ["1", "0", ...]  } }
  auto results = resultResponse["result"];

  cudaq::info("Results message: {}", results.dump());

  std::vector<ExecutionResult> srs;

  // Populate individual registers' results into srs
  for (auto &[registerName, result] : results.items()) {
    auto bitResults = result.get<std::vector<std::string>>();
    CountsDictionary thisRegCounts;
    for (auto &b : bitResults)
      thisRegCounts[b]++;
    srs.emplace_back(thisRegCounts, registerName);
    srs.back().sequentialData = bitResults;
  }

  // The global register needs to have results sorted by qubit number.
  // Sort output_names by qubit first and then result number. If there are
  // duplicate measurements for a qubit, only save the last one.
  if (outputNames.find(jobId) == outputNames.end())
    throw std::runtime_error("Could not find output names for job " + jobId);

  auto &output_names = outputNames[jobId];
  for (auto &[result, info] : output_names) {
    cudaq::info("Qubit {} Result {} Name {}", info.qubitNum, result,
                info.registerName);
  }

  // The local mock server tests don't work the same way as the true MQSS
  // QPU. They do not support the full named QIR output recording functions.
  // Detect for the that difference here.
  bool mockServer = false;
  if (results.begin().key() == "MOCK_SERVER_RESULTS")
    mockServer = true;

  if (!mockServer)
    for (auto &[_, val] : output_names)
      if (!results.contains(val.registerName))
        throw std::runtime_error("Expected to see " + val.registerName +
                                 " in the results, but did not see it.");

  // Construct idx[] such that output_names[idx[:]] is sorted by QIR qubit
  // number. There may initially be duplicate qubit numbers if that qubit was
  // measured multiple times. If that's true, make the lower-numbered result
  // occur first. (Dups will be removed in the next step below.)
  std::vector<std::size_t> idx;
  if (!mockServer) {
    idx.resize(output_names.size());
    std::iota(idx.begin(), idx.end(), 0);
    std::sort(idx.begin(), idx.end(), [&](std::size_t i1, std::size_t i2) {
      if (output_names[i1].qubitNum == output_names[i2].qubitNum)
        return i1 < i2; // choose lower result number
      return output_names[i1].qubitNum < output_names[i2].qubitNum;
    });

    // The global register only contains the *final* measurement of each
    // requested qubit, so eliminate lower-numbered results from idx array.
    for (auto it = idx.begin(); it != idx.end();) {
      if (std::next(it) != idx.end()) {
        if (output_names[*it].qubitNum ==
            output_names[*std::next(it)].qubitNum) {
          it = idx.erase(it);
          continue;
        }
      }
      ++it;
    }
  } else {
    idx.resize(1); // local mock server tests
  }

  // For each shot, we concatenate the measurements results of all qubits.
  auto begin = results.begin();
  auto nShots = begin.value().get<std::vector<std::string>>().size();
  std::vector<std::string> bitstrings(nShots);
  for (auto r : idx) {
    // If allNamesPresent == false, that means we are running local mock server
    // tests which don't support the full QIR output recording functions. Just
    // use the first key in that case.
    auto bitResults =
        mockServer ? results.at(begin.key()).get<std::vector<std::string>>()
                   : results.at(output_names[r].registerName)
                         .get<std::vector<std::string>>();
    for (size_t i = 0; auto &bit : bitResults)
      bitstrings[i++] += bit;
  }

  cudaq::CountsDictionary counts;
  for (auto &b : bitstrings)
    counts[b]++;

  // Store the combined results into the global register
  srs.emplace_back(counts, GlobalRegisterName);
  srs.back().sequentialData = bitstrings;
  return sample_result(srs);
}

std::map<std::string, std::string>
MQSSServerHelper::generateRequestHeader() const {
  std::map<std::string, std::string> headers{
      {"Authorization", "Bearer "+token}};
  return headers;
}

RestHeaders MQSSServerHelper::getHeaders() {
  return generateRequestHeader();
}

void findMQSSApiKeyInFile(std::string &apiKey, const std::string &path,
                      std::string &refreshKey, std::string &timeStr) {
  std::ifstream stream(path);
  std::string contents((std::istreambuf_iterator<char>(stream)),
                       std::istreambuf_iterator<char>());

  std::vector<std::string> lines;
  lines = cudaq::split(contents, '\n');
  for (const std::string &l : lines) {
    std::vector<std::string> keyAndValue = cudaq::split(l, ':');
    if (keyAndValue.size() != 2)
      throw std::runtime_error("Ill-formed configuration file (" + path +
                               "). Key-value pairs must be in `<key> : "
                               "<value>` format. (One per line)");
    cudaq::trim(keyAndValue[0]);
    cudaq::trim(keyAndValue[1]);
    if (keyAndValue[0] == "key")
      apiKey = keyAndValue[1];
    else if (keyAndValue[0] == "refresh")
      refreshKey = keyAndValue[1];
    else if (keyAndValue[0] == "time")
      timeStr = keyAndValue[1];
    else
      throw std::runtime_error(
          "Unknown key in configuration file: " + keyAndValue[0] + ".");
  }
  if (apiKey.empty())
    throw std::runtime_error("Empty API key in configuration file (" + path +
                             ").");
  if (refreshKey.empty())
    throw std::runtime_error("Empty refresh key in configuration file (" +
                             path + ").");
  // The `time` key is not required.
}

/// Search for the API key
std::string searchMQSSAPIKey(std::string &key, std::string &refreshKey,
                         std::string &timeStr,
                         std::string userSpecifiedConfig) {
  std::string hwConfig;
  // Allow someone to tweak this with an environment variable
  if (auto creds = std::getenv("CUDAQ_MQSS_CONFIGURATION"))
    hwConfig = std::string(creds);
  else if (!userSpecifiedConfig.empty())
    hwConfig = userSpecifiedConfig;
  else
    hwConfig = std::string(getenv("HOME")) + std::string("/.mqss_config");
  if (cudaq::fileExists(hwConfig)) {
    findMQSSApiKeyInFile(key, hwConfig, refreshKey, timeStr);
  } else {
    throw std::runtime_error(
        "Cannot find MQSS Config file with credentials "
        "(~/.mqssMQP_config).");
  }
  return hwConfig;
}
} // namespace cudaq

CUDAQ_REGISTER_TYPE(cudaq::ServerHelper, cudaq::MQSSServerHelper,
                    mqssMQP)
