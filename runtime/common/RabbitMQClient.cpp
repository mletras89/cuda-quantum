/*******************************************************************************
 * Copyright (c) 2022 - 2024 NVIDIA Corporation & Affiliates.                  *
 * All rights reserved.                                                        *
 *                                                                             *
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/

#include "common/Logger.h"
#include "RabbitMQClient.h"
#include "cudaq/utils/cudaq_utils.h"
#include <cpr/cpr.h>
#include <zlib.h>
#include <iostream>

std::string generate_uuid() {
    uuid_t uuid;
    uuid_generate(uuid);
    char str[37];
    uuid_unparse(uuid, str);
    return std::string(str);
}

namespace mqss {

void RabbitMQClient::setupConnection(){
  conn = amqp_new_connection();
  socket = amqp_tcp_socket_new(conn);
  if (!socket)
    throw std::runtime_error("MQSS: Failed to create TCP socket");
  if (amqp_socket_open(socket, RABBITMQ_SERVER_ADDRESS, RABBITMQ_CUDAQ_PORT))
    throw std::runtime_error("MQSS: Failed to open TCP socket");

  amqp_login(conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, RABBITMQ_USER, RABBITMQ_PASSWORD);
  amqp_channel_open(conn, 1);
  amqp_get_rpc_reply(conn);
}

RabbitMQClient::RabbitMQClient() : conn(amqp_new_connection()), 
                                   socket(nullptr){
  setupConnection();
  nlohmann::json jsonObject = {
      {"auth", "Authentication"},
  };
  std::cout << "Sending login" << std::endl;
  std::string answer = sendMessageWithReply(RABBITMQ_CUDAQ_LOGIN_QUEUE,jsonObject.dump(),true);
  nlohmann::json json_answer = nlohmann::json::parse(answer);
  std::cout << "Initializing rabbitmq client with answer" << json_answer.dump() << std::endl;
}

RabbitMQClient::~RabbitMQClient(){
  RabbitMQClient::closeConnection();
}


void RabbitMQClient::closeConnection() {
  amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS);
  amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
  amqp_destroy_connection(conn);
}

std::string RabbitMQClient::getMessageFromReplyQueue(const std::string& correlation_id,
                                                     const std::string& reply_queue){
  // Listen for response
  std::cout << "Correlation id:" << correlation_id <<std::endl;
  std::cout << "replyQueue: " << reply_queue <<std::endl;
  amqp_basic_consume(conn, 1, amqp_cstring_bytes(reply_queue.c_str()), amqp_empty_bytes,
                      0, 1, 0, amqp_empty_table);

  while (true) {
    amqp_rpc_reply_t res;
    amqp_envelope_t envelope;
    amqp_maybe_release_buffers(conn);
    res = amqp_consume_message(conn, &envelope, NULL, 0);

    if (res.reply_type != AMQP_RESPONSE_NORMAL)
      continue;
    if (!(envelope.message.properties._flags & AMQP_BASIC_CORRELATION_ID_FLAG))
      continue;
    std::string received_id((char*)envelope.message.properties.correlation_id.bytes,
                                   envelope.message.properties.correlation_id.len);
    std::cout << "Correlation id a:" << correlation_id <<std::endl;
    std::cout << "received id:" << correlation_id <<std::endl;
    if (received_id != correlation_id)
      continue;

    // Retrieve and process answer
    std::string message = std::string((char*)envelope.message.body.bytes,
                                             envelope.message.body.len);
    amqp_destroy_envelope(&envelope);
    return message;
  }
  return "";
}

// send message via rabbit-mq, if isJSON is true, the string message is treated as json string
std::string RabbitMQClient::sendMessageWithReply(const std::string& request_queue,
                                                 const std::string& message,
                                                 bool isJSON){
  std::string correlation_id = generate_uuid();

  amqp_basic_properties_t props;
  props._flags =  AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_DELIVERY_MODE_FLAG |
                  AMQP_BASIC_CORRELATION_ID_FLAG | AMQP_BASIC_REPLY_TO_FLAG;
  props.content_type = amqp_cstring_bytes("text/plain");  // sending message as string
  if (isJSON)
    props.content_type = amqp_cstring_bytes("application/json"); // specify to send json

  props.delivery_mode = 2;  // Persistent delivery mode
  props.correlation_id = amqp_cstring_bytes(correlation_id.c_str());
  
  // Listen for response
  amqp_queue_declare_ok_t* replyQueue = amqp_queue_declare(conn, 1, amqp_empty_bytes, 0, 0, 0, 1, amqp_empty_table);
  std::string reply_queue = std::string((char*)replyQueue->queue.bytes, replyQueue->queue.len);

  props.reply_to = amqp_cstring_bytes(reply_queue.c_str());

  std::cout << "before publish" << std::endl;
  // send the message
  amqp_basic_publish(conn, 1, amqp_empty_bytes, amqp_cstring_bytes(request_queue.c_str()),
                      0, 0, &props, amqp_cstring_bytes(message.c_str()));
  std::cout << "after publish" << std::endl;
  // getting the message from reply queue
  std::cout << "Correlation id " << correlation_id << std::endl;
  return getMessageFromReplyQueue(correlation_id,reply_queue);
}
} // namespace mqss
