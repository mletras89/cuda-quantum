/*-------------------------------------------------------------------------
 This code and any associated documentation is provided "as is"

 IN NO EVENT SHALL LEIBNIZ-RECHENZENTRUM (LRZ) BE LIABLE TO ANY PARTY FOR
 DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
 OF THE USE OF THIS CODE AND ITS DOCUMENTATION, EVEN IF LEIBNIZ-RECHENZENTRUM
 (LRZ) HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 THE AFOREMENTIONED EXCLUSIONS OF LIABILITY DO NOT APPLY IN CASE OF INTENT
 BY LEIBNIZ-RECHENZENTRUM (LRZ).

 LEIBNIZ-RECHENZENTRUM (LRZ), SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 FOR A PARTICULAR PURPOSE.

 THE CODE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, LEIBNIZ-RECHENZENTRUM (LRZ)
 HAS NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
 MODIFICATIONS.
 -------------------------------------------------------------------------

@author Martin Letras
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
