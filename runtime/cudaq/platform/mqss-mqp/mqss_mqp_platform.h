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
  Headers used to define the namespace cudaq::mqss. 
  Here, methods to configure CUDAQ and set the MQSS MQP as target backend.
  Moreover, all the sample methods are encapsulated in the mqss namespace.
  E.g., sample can also be called as cudaq::mqss::sample or just sample.

 *******************************************************************************
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/

#pragma once

#ifndef MQSS_MQP_PLATFORM_H
#define MQSS_MQP_PLATFORM_H

#include "cudaq.h"
#include <functional>

#define MQSS_MQP_URL      "http://localhost"
#define MQSS_MQP_PORT     62440

const std::string backendStringTemplate = "mqssMQP;emulate;false;url;{}:{};credentials;{}";

namespace cudaq::mqss {

void loadFakeConfigFile();

void removeFakeConfigFile();

void setTargetBackend();

void setTargetBackend(const std::string& configFile, 
                      const std::string& URL, 
                      int port);

void setTargetBackend(const std::string& config);

// Forwarding all the sample methods
/// @brief Implementation of the sample method of the cudaq::mqss namespace
#if CUDAQ_USE_STD20
template <typename QuantumKernel, typename... Args>
  requires SampleCallValid<QuantumKernel, Args...>
#else
template <typename QuantumKernel, typename... Args,
          typename = std::enable_if_t<
              std::is_invocable_r_v<void, QuantumKernel, Args...>>>
#endif
cudaq::sample_result sample(QuantumKernel &&kernel, Args &&...args); 

#if CUDAQ_USE_STD20
template <typename QuantumKernel, typename... Args>
  requires SampleCallValid<QuantumKernel, Args...>
#else
template <typename QuantumKernel, typename... Args,
          typename = std::enable_if_t<
              std::is_invocable_r_v<void, QuantumKernel, Args...>>>
#endif
auto sample(std::size_t shots, QuantumKernel &&kernel, Args &&...args);

#if CUDAQ_USE_STD20
template <typename QuantumKernel, typename... Args>
  requires SampleCallValid<QuantumKernel, Args...>
#else
template <typename QuantumKernel, typename... Args,
          typename = std::enable_if_t<
              std::is_invocable_r_v<void, QuantumKernel, Args...>>>
#endif
sample_result sample(const sample_options &options, QuantumKernel &&kernel,
                     Args &&...args);

#if CUDAQ_USE_STD20
template <typename QuantumKernel, typename... Args>
  requires SampleCallValid<QuantumKernel, Args...>
#else
template <typename QuantumKernel, typename... Args,
          typename = std::enable_if_t<
              std::is_invocable_r_v<void, QuantumKernel, Args...>>>
#endif
async_sample_result sample_async(const std::size_t qpu_id,
                                 QuantumKernel &&kernel, Args &&...args);                     
#if CUDAQ_USE_STD20
template <typename QuantumKernel, typename... Args>
  requires SampleCallValid<QuantumKernel, Args...>
#else
template <typename QuantumKernel, typename... Args,
          typename = std::enable_if_t<
              std::is_invocable_r_v<void, QuantumKernel, Args...>>>
#endif
async_sample_result sample_async(std::size_t shots, std::size_t qpu_id,
                                 QuantumKernel &&kernel, Args &&...args);

#if CUDAQ_USE_STD20
template <typename QuantumKernel, typename... Args>
  requires SampleCallValid<QuantumKernel, Args...>
#else
template <typename QuantumKernel, typename... Args,
          typename = std::enable_if_t<
              std::is_invocable_r_v<void, QuantumKernel, Args...>>>
#endif
auto sample_async(QuantumKernel &&kernel, Args &&...args);

#if CUDAQ_USE_STD20
template <typename QuantumKernel, typename... Args>
  requires SampleCallValid<QuantumKernel, Args...>
#else
template <typename QuantumKernel, typename... Args,
          typename = std::enable_if_t<
              std::is_invocable_r_v<void, QuantumKernel, Args...>>>
#endif
std::vector<sample_result> sample(QuantumKernel &&kernel,
                                  ArgumentSet<Args...> &&params);

#if CUDAQ_USE_STD20
template <typename QuantumKernel, typename... Args>
  requires SampleCallValid<QuantumKernel, Args...>
#else
template <typename QuantumKernel, typename... Args,
          typename = std::enable_if_t<
              std::is_invocable_r_v<void, QuantumKernel, Args...>>>
#endif
std::vector<sample_result> sample(std::size_t shots, QuantumKernel &&kernel,
                                  ArgumentSet<Args...> &&params);

#if CUDAQ_USE_STD20
template <typename QuantumKernel, typename... Args>
  requires SampleCallValid<QuantumKernel, Args...>
#else
template <typename QuantumKernel, typename... Args,
          typename = std::enable_if_t<
              std::is_invocable_r_v<void, QuantumKernel, Args...>>>
#endif
std::vector<sample_result> sample(const sample_options &options,
                                  QuantumKernel &&kernel,
                                  ArgumentSet<Args...> &&params);

// Forwarding all the observe methods


}; // namespace cudaq::mqss

#endif
