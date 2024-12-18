/*******************************************************************************
 * Copyright (c) 2022 - 2024 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/

#include "Future.h"
#include "Logger.h"
#include "ObserveResult.h"
#include "RestClient.h"
#include "RabbitMQClient.h"
#include "ServerHelper.h"
#include <thread>
#include <iostream>

namespace cudaq::details {

sample_result future::get() {
  if (wrapsFutureSampling)
    return inFuture.get();
  bool isMQSSTargetBackend = false;

#ifdef CUDAQ_RESTCLIENT_AVAILABLE
  mqss::RabbitMQClient* rabbitMQClient; 
  RestClient client;
  auto serverHelper = registry::get<ServerHelper>(qpuName);
  serverHelper->initialize(serverConfig);
  auto headers = serverHelper->getHeaders();
  std::cout << "server helper "<< serverHelper->name() << std::endl;
  // for adding support for rabbitmq-mqss
  if (serverHelper->name().find("mqssHPC") != std::string::npos){
    rabbitMQClient = new mqss::RabbitMQClient();
    isMQSSTargetBackend = true;
  }
  std::vector<ExecutionResult> results;
  for (auto &id : jobs) {
    cudaq::info("Future retrieving results for {}.", id.first);

    auto jobGetPath = serverHelper->constructGetJobPath(id.first);

    cudaq::info("Future got job retrieval path as {}.", jobGetPath);

    nlohmann::json resultResponse;
    // This have to be added to support rabbitmq
    if (isMQSSTargetBackend){
      std::string response_str = rabbitMQClient->sendMessageWithReply(RABBITMQ_CUDAQ_JOBSTRING_QUEUE,id.first,false);
      resultResponse = nlohmann::json::parse(response_str);
    }
    else
      resultResponse = client.get(jobGetPath, "", headers);
    while (!serverHelper->jobIsDone(resultResponse)) {
      auto polling_interval =
          serverHelper->nextResultPollingInterval(resultResponse);
      std::this_thread::sleep_for(polling_interval);
      // This have to be added to support rabbitmq
      if (isMQSSTargetBackend){
        std::string response_str = rabbitMQClient->sendMessageWithReply(RABBITMQ_CUDAQ_JOBSTRING_QUEUE,id.first,false);
        resultResponse = nlohmann::json::parse(response_str);
      }
      else
        resultResponse = client.get(jobGetPath, "", headers);
    }
    auto c = serverHelper->processResults(resultResponse, id.first);

    // If there are multiple jobs, this is likely a spin_op.
    // If so, use the job name instead of the global register.
    if (jobs.size() > 1) {
      results.emplace_back(c.to_map(), id.second);
      results.back().sequentialData = c.sequential_data();
    } else {
      // For each register, add the results into result.
      for (auto &regName : c.register_names()) {
        results.emplace_back(c.to_map(regName), regName);
        results.back().sequentialData = c.sequential_data(regName);
      }
    }
  }

  return sample_result(results);
#else
  throw std::runtime_error("cudaq::details::future::get() requires REST Client "
                           "but CUDA-Q was built without it.");
  return sample_result();
#endif
}

future &future::operator=(future &other) {
  jobs = other.jobs;
  qpuName = other.qpuName;
  serverConfig = other.serverConfig;
  if (other.wrapsFutureSampling) {
    wrapsFutureSampling = true;
    inFuture = std::move(other.inFuture);
  }
  return *this;
}

future &future::operator=(future &&other) {
  jobs = other.jobs;
  qpuName = other.qpuName;
  serverConfig = other.serverConfig;
  if (other.wrapsFutureSampling) {
    wrapsFutureSampling = true;
    inFuture = std::move(other.inFuture);
  }
  return *this;
}

std::ostream &operator<<(std::ostream &os, future &f) {
  if (f.wrapsFutureSampling)
    throw std::runtime_error(
        "Cannot persist a cudaq::future for a local kernel execution.");

  nlohmann::json j;
  j["jobs"] = f.jobs;
  j["qpu"] = f.qpuName;
  j["config"] = f.serverConfig;
  os << j.dump(4);
  return os;
}

std::istream &operator>>(std::istream &is, future &f) {
  nlohmann::json j;
  try {
    is >> j;
  } catch (...) {
    throw std::runtime_error(
        "Formatting error; could not parse input as json.");
  }
  f.jobs = j["jobs"].get<std::vector<future::Job>>();
  f.qpuName = j["qpu"].get<std::string>();
  f.serverConfig = j["config"].get<std::map<std::string, std::string>>();
  return is;
}

} // namespace cudaq::details
