/*******************************************************************************
 * Copyright (c) 2022 - 2024 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/

#include "Executor.h"
#include "common/Logger.h"

namespace cudaq {
details::future
Executor::execute(std::vector<KernelExecution> &codesToExecute) {
  bool isMQSSTargetBackend = false;
  serverHelper->setShots(shots);

  cudaq::info("Executor creating {} jobs to execute with the {} helper.",
              codesToExecute.size(), serverHelper->name());

  // Create the Job Payload, composed of job post path, headers,
  // and the job json messages themselves
  auto [jobPostPath, headers, jobs] = serverHelper->createJob(codesToExecute);
  auto config = serverHelper->getConfig();
  std::vector<details::future::Job> ids;

  // for adding support for rabbitmq-mqss
  if (serverHelper->name().find("mqssHPC") != std::string::npos){ 
    isMQSSTargetBackend = true;
    rabbitMQClient = new mqss::RabbitMQClient();
  }

  for (std::size_t i = 0; auto &job : jobs) {
    cudaq::info("Job (name={}) created, posting to {}", codesToExecute[i].name,
                jobPostPath);
    nlohmann::json response;
    // Post it, get the response
    if (isMQSSTargetBackend){
      std::string response_str = rabbitMQClient->sendMessageWithReply(RABBITMQ_CUDAQ_JOB_QUEUE,job.dump(),true);
      response = nlohmann::json::parse(response_str);
    }
    else
      response = client.post(jobPostPath, "", job, headers);
    cudaq::info("Job (name={}) posted, response was {}", codesToExecute[i].name,
                response.dump());

    // Add the job id and the job name.
    auto task_id = serverHelper->extractJobId(response);
    if (task_id.empty()) {
      nlohmann::json tmp(job.at("tasks"));
      serverHelper->constructGetJobPath(tmp[0]);
      task_id = tmp[0].at("task_id");
    }
    cudaq::info("Task ID is {}", task_id);
    ids.emplace_back(task_id, codesToExecute[i].name);
    config["output_names." + task_id] = codesToExecute[i].output_names.dump();

    nlohmann::json jReorder = codesToExecute[i].mapping_reorder_idx;
    config["reorderIdx." + task_id] = jReorder.dump();

    i++;
  }

  config.insert({"shots", std::to_string(shots)});
  std::string name = serverHelper->name();
  return details::future(ids, name, config);
}
} // namespace cudaq
