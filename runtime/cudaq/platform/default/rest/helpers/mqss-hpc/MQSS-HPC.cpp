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
  Stack (MQSS). The communication is done via rabbitmq to cover the case
  of HPCQC integration.

*******************************************************************************
* This source code and the accompanying materials are made available under    *
* the terms of the Apache License 2.0 which accompanies this distribution.    *
******************************************************************************/

#include "common/Logger.h"
#include "common/ServerHelper.h"
#include "cudaq/utils/cudaq_utils.h"
#include "common/RabbitMQClient.h"
#include "common/QuantumTask.h"
#include <fstream>
#include <iostream>
#include <thread>

namespace cudaq {

/// @brief Find and set the API and refresh tokens, and the time string.
void findMQSSApiKeyInFileHPC(std::string &apiKey, const std::string &path,
                      std::string &refreshKey, std::string &timeStr);

/// Search for the API key, invokes findMQSSApiKeyInFile
std::string searchMQSSAPIKeyHPC(std::string &key, std::string &refreshKey,
                         std::string &timeStr,
                         std::string userSpecifiedConfig = "");

/// @brief The MQSSServerHelper implements the ServerHelper interface
/// to map Job requests and Job result retrievals actions from the calling
/// Executor to the specific schema required by the remote MQSS REST
/// server.
class HPCServerHelper : public ServerHelper {
protected:
  /// @brief The base URL
  std::string baseUrl = "https://qapi.quantinuum.com/v1/";
  /// @brief The machine we are targeting
  std::string machine = "H2-1SC";
  /// @brief Time string, when the last tokens were retrieved
  std::string timeStr = "";
  /// @brief The refresh token
  std::string refreshKey = "";
  /// @brief The API token for the remote server
  std::string apiKey = "";

  std::string userSpecifiedCredentials = "";
  std::string credentialsPath = "";

  /// @brief MQSS requires the API token be updated every so often,
  /// using the provided refresh token. This function will do that.
  void refreshTokens(bool force_refresh = false);

  // information read from the configuration that has to be passed to the MQSS
  QuantumJob quantumTask;
  /// @brief Return the headers required for the REST calls
  RestHeaders generateRequestHeader() const;

public:
  /// @brief Return the name of this server helper, must be the
  /// same as the qpu config file.
  const std::string name() const override { return "mqssHPC"; }
  RestHeaders getHeaders() override;

  void initialize(BackendConfig config) override {
    backendConfig = config;

    // Set the machine
    auto iter = backendConfig.find("machine");
    if (iter != backendConfig.end())
      machine = iter->second;

    // Set an alternate base URL if provided
    iter = backendConfig.find("url");
    if (iter != backendConfig.end()) {
      baseUrl = iter->second;
      if (!baseUrl.ends_with("/"))
        baseUrl += "/";
    }

    iter = backendConfig.find("credentials");
    if (iter != backendConfig.end())
      userSpecifiedCredentials = iter->second;

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

    parseConfigForCommonParams(config);
  }

  /// @brief Create a job payload for the provided quantum codes
  ServerJobPayload
  createJob(std::vector<KernelExecution> &circuitCodes) override;

  /// @brief Return the job id from the previous job post
  std::string extractJobId(ServerMessage &postResponse) override;

  /// @brief Return the URL for retrieving job results
  std::string constructGetJobPath(ServerMessage &postResponse) override;
  std::string constructGetJobPath(std::string &jobId) override;

  /// @brief Return true if the job is done
  bool jobIsDone(ServerMessage &getJobResponse) override;

  /// @brief Given a completed job response, map back to the sample_result
  cudaq::sample_result processResults(ServerMessage &postJobResponse,
                                      std::string &jobID) override;
};

ServerJobPayload
HPCServerHelper::createJob(std::vector<KernelExecution> &circuitCodes) {
  
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
    j["via_hpc"] = true; // via MQP
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

  // Get the tokens we need
  credentialsPath =
      searchMQSSAPIKeyHPC(apiKey, refreshKey, timeStr, userSpecifiedCredentials);
  refreshTokens();

  // Get the headers
  RestHeaders headers = generateRequestHeader();

  cudaq::info(
      "Created job payload for MQSS, language is quake, targeting {}",
      machine);
   // return the payload
  return std::make_tuple(baseUrl + "job", headers, messages);
}

std::string HPCServerHelper::extractJobId(ServerMessage &postResponse) {
  return postResponse["job"].get<std::string>();
}

std::string
HPCServerHelper::constructGetJobPath(ServerMessage &postResponse) {
  return baseUrl + "job/" + extractJobId(postResponse);
}

std::string HPCServerHelper::constructGetJobPath(std::string &jobId) {
  return baseUrl + "job/" + jobId;
}

bool HPCServerHelper::jobIsDone(ServerMessage &getJobResponse) {
  auto status = getJobResponse["status"].get<std::string>();
  if (status == "failed") {
    std::string msg = "";
    if (getJobResponse.count("error"))
      msg = getJobResponse["error"]["text"].get<std::string>();
    throw std::runtime_error("Job failed to execute msg = [" + msg + "]");
  }

  return status == "completed";
}

cudaq::sample_result
HPCServerHelper::processResults(ServerMessage &postJobResponse,
                                       std::string &jobId) {
  // Results come back as a map of vectors. Each map key corresponds to a qubit
  // and its corresponding vector holds the measurement results in each shot:
  //      { "results" : { "r0" : ["0", "0", ...],
  //                      "r1" : ["1", "0", ...]  } }
  auto results = postJobResponse["results"];

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
HPCServerHelper::generateRequestHeader() const {
  std::string apiKey, refreshKey, timeStr;
  searchMQSSAPIKeyHPC(apiKey, refreshKey, timeStr, userSpecifiedCredentials);
  std::map<std::string, std::string> headers{
      {"Authorization", apiKey},
      {"Content-Type", "application/json"},
      {"Connection", "keep-alive"},
      {"Accept", "*/*"}};
  return headers;
}

RestHeaders HPCServerHelper::getHeaders() {
  return generateRequestHeader();
}

/// Refresh the api key and refresh-token
void HPCServerHelper::refreshTokens(bool force_refresh) {
  std::mutex m;
  std::lock_guard<std::mutex> l(m);
  ::mqss::RabbitMQClient client;
  auto now = std::chrono::high_resolution_clock::now();

  // If the time string is empty, let's add it
  if (timeStr.empty()) {
    timeStr = std::to_string(now.time_since_epoch().count());
    std::ofstream out(credentialsPath);
    out << "key:" << apiKey << '\n';
    out << "refresh:" << refreshKey << '\n';
    out << "time:" << timeStr << '\n';
  }

  // We first check how much time has elapsed since the
  // existing refresh key was created
  std::int64_t timeAsLong = std::stol(timeStr);
  std::chrono::high_resolution_clock::duration d(timeAsLong);
  std::chrono::high_resolution_clock::time_point oldTime(d);
  auto secondsDuration =
      1e-3 *
      std::chrono::duration_cast<std::chrono::milliseconds>(now - oldTime);

  // If we are getting close to an 30 min, then we will refresh
  bool needsRefresh = secondsDuration.count() * (1. / 1800.) > .85;
  if (needsRefresh || force_refresh) {
    cudaq::info("Refreshing id-token");
    std::stringstream ss;
    ss << "\"refresh-token\":\"" << refreshKey << "\"";
    auto headers = generateRequestHeader();
    nlohmann::json j;
    j["refresh-token"] = refreshKey;
    /*std::string response_str = client.sendMessageWithReply(RABBITMQ_CUDAQ_LOGIN_QUEUE, j.dump(), true);
    nlohmann::json response_json = nlohmann::json::parse(response_str);
    apiKey = response_json["id-token"].get<std::string>();
    refreshKey = response_json["refresh-token"].get<std::string>();
    std::ofstream out(credentialsPath);
    out << "key:" << apiKey << '\n';
    out << "refresh:" << refreshKey << '\n';
    out << "time:" << now.time_since_epoch().count() << '\n';*/
    timeStr = std::to_string(now.time_since_epoch().count());
  }
}

void findMQSSApiKeyInFileHPC(std::string &apiKey, const std::string &path,
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
std::string searchMQSSAPIKeyHPC(std::string &key, std::string &refreshKey,
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
    findMQSSApiKeyInFileHPC(key, hwConfig, refreshKey, timeStr);
  } else {
    throw std::runtime_error(
        "Cannot find MQSS Config file with credentials "
        "(~/.mqss_config).");
  }
  return hwConfig;
}

} // namespace cudaq

CUDAQ_REGISTER_TYPE(cudaq::ServerHelper, cudaq::HPCServerHelper, 
                    mqssHPC)
