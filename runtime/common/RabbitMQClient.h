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
  This header defines the RabbitMQ Client class, used to communicate CudaQ 
  via RabitMQ to the Munich Quantum Software Stack (MQSS)

 *******************************************************************************
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/

#pragma once
#include "nlohmann/json.hpp"
#include <map>
#include <string>
#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/tcp_socket.h>
#include <cstdlib>
#include <filesystem>
#include <uuid/uuid.h>  // For generating unique correlation IDs

#define RABBITMQ_SERVER_ADDRESS     "127.0.0.1"
#define RABBITMQ_CUDAQ_PORT         5672
#define RABBITMQ_USER               "guest"
#define RABBITMQ_PASSWORD           "guest"

#define RABBITMQ_CUDAQ_LOGIN_QUEUE        "/login"
#define RABBITMQ_CUDAQ_JOB_QUEUE          "/job"
#define RABBITMQ_CUDAQ_JOBSTRING_QUEUE    "/job/<string>"

namespace mqss {

/// @brief The RabbitMQ client exposes an interface
/// to communicate to MQSS
class RabbitMQClient {
protected:
  // Use verbose printout
  bool verbose = false;
  
  const std::string loginQueue      = RABBITMQ_CUDAQ_LOGIN_QUEUE;
  const std::string jobQueue        = RABBITMQ_CUDAQ_JOB_QUEUE;
  const std::string jobStringQueue  = RABBITMQ_CUDAQ_JOBSTRING_QUEUE;

  std::string getMessageFromReplyQueue(const std::string& correlation_id,
                                       const std::string& reply_queue);
  
  void setupConnection();
  void closeConnection();

  amqp_connection_state_t conn;
  amqp_socket_t* socket;
public:
  /// @brief set verbose printout
  /// @param v
  void setVerbose(bool v) { verbose = v; }

  /// @brief Constructor
  RabbitMQClient();

  /// @brief Destructor
  ~RabbitMQClient();

  std::string sendMessageWithReply(const std::string& request_queue,
                                   const std::string& message,
                                   bool isJSON=false);
};
} // namespace mqss
