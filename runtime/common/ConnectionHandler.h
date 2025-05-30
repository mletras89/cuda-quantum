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
  @date   May 2025
  @version 1.0
  @ brief
    Header file containing constants used for communications via
    RabbitMQ
 *******************************************************************************
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/
#pragma once

// Define the RabbitMQ server connection information
#define AMQP_SERVER "127.0.0.1" //"localhost"
#define AMQP_PORT 5672
#define AMQP_USER "guest"
#define AMQP_PASSWORD "guest"
#define AMQP_VHOST "/"
#define RESPONSESIZE 150

#define QUEUE_HPC_OFFLOADER "MQSS-HPC-Offloader"
#define QUEUE_MQP_OFFLOADER "MQSS-MQP-Offloader"
#define QUEUE_QRM_AGNOSTIC_PASS_RUNNER "MQSS-To-AgnosticPassRunner"
#define QUEUE_AGNOSTIC_PASS_RUNNER_SCHEDULER "MQSS-To-Scheduler"
#define QUEUE_SCHEDULER_TRANSPILER "MQSS-To-Transpiler"
#define QUEUE_TRANSPILER_SUBMITTER "MQSS-To-Submitter"
#define QUEUE_SUBMITTER_OFFLOADER "MQSS-To-Offloader"
#define QUEUE_SUBMITTER_BACKEND "MQSS-To-Backend" // for Testing
#define QUEUE_BACKEND_SUBMITTER "MQSS-To-Backend" // for Testing
