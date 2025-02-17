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
  Implementation of the namespace cudaq::mqss. 
  Here, methods to fine configure CUDAQ and set the MQSS MQP as target backend.
  E.g., the url and the port of the REST server might be set here, too.
  Moreover, all the sample methods are encapsulated in the mqss namespace.
  E.g., sample can also be called as cudaq::mqss::sample or just sample.

 *******************************************************************************
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/
#include "mqss_mqp_platform.h"
#include "common/FmtCore.h"
#include "cudaq/algorithms/sample.h"
#include "cudaq/concepts.h"
#include <fstream>
#include <utility>
#include <iostream>
namespace cudaq::mqss {

void loadFakeConfigFile(){
  std::string home = std::getenv("HOME");
  std::string fileName = home + "/FakeCppMQSS.config";
  std::ofstream out(fileName);
  out << "key: key\nrefresh: refresh\ntime: 0";
  out.close();
}

void removeFakeConfigFile(){
  std::string home = std::getenv("HOME");
  std::string fileName = home + "/FakeCppMQSS.config";
  std::remove(fileName.c_str());
}

void setTargetBackend(const std::string& configFile,
                      const std::string& URL,
                      int port){
  auto backendString =
      fmt::format(fmt::runtime(backendStringTemplate), URL, port, configFile);

  auto &platform = cudaq::get_platform();
  platform.setTargetBackend(backendString);
}

void setTargetBackend(const std::string& config){
  auto backendString =
      fmt::format(fmt::runtime(backendStringTemplate), std::string(MQSS_MQP_URL), MQSS_MQP_PORT, config);

  auto &platform = cudaq::get_platform();
  platform.setTargetBackend(backendString);
}

void setTargetBackend(){
  std::cout << "Setting target backend..." << std::endl;
//  std::string home = std::getenv("HOME");
//  std::string fileName = home + "/FakeCppMQSS.config";
//  loadFakeConfigFile();
//  auto backendString =      fmt::format(fmt::runtime(backendStringTemplate), std::string(MQSS_MQP_URL), MQSS_MQP_PORT, fileName);
//  std::cout << "backendString " << backendString << std::endl;
//  auto &platform = cudaq::get_platform();
//  platform.setTargetBackend(backendString);
//  std::cout << "Platform " << platform.name() << std::endl;

}

/// @brief Implementation of the sample method of the cudaq::mqss namespace
#if CUDAQ_USE_STD20
template <typename QuantumKernel, typename... Args>
  requires SampleCallValid<QuantumKernel, Args...>
#else
template <typename QuantumKernel, typename... Args,
          typename = std::enable_if_t<
              std::is_invocable_r_v<void, QuantumKernel, Args...>>>
#endif
cudaq::sample_result sample(QuantumKernel &&kernel, Args &&...args){
  return cudaq::sample(std::forward<QuantumKernel>(kernel),  std::forward<Args>(args)...);
}

#if CUDAQ_USE_STD20
template <typename QuantumKernel, typename... Args>
  requires SampleCallValid<QuantumKernel, Args...>
#else
template <typename QuantumKernel, typename... Args,
          typename = std::enable_if_t<
              std::is_invocable_r_v<void, QuantumKernel, Args...>>>
#endif
auto sample(std::size_t shots, QuantumKernel &&kernel, Args &&...args){
  return cudaq::sample(std::forward<std::size_t>(shots), std::forward<QuantumKernel>(kernel), std::forward<Args>(args)...);
}

#if CUDAQ_USE_STD20
template <typename QuantumKernel, typename... Args>
  requires SampleCallValid<QuantumKernel, Args...>
#else
template <typename QuantumKernel, typename... Args,
          typename = std::enable_if_t<
              std::is_invocable_r_v<void, QuantumKernel, Args...>>>
#endif
sample_result sample(const sample_options &options, QuantumKernel &&kernel,
                     Args &&...args){
  return cudaq::sample(options, std::forward<QuantumKernel>(kernel),
                       std::forward<Args>(args)...);
}

#if CUDAQ_USE_STD20
template <typename QuantumKernel, typename... Args>
  requires SampleCallValid<QuantumKernel, Args...>
#else
template <typename QuantumKernel, typename... Args,
          typename = std::enable_if_t<
              std::is_invocable_r_v<void, QuantumKernel, Args...>>>
#endif
async_sample_result sample_async(const std::size_t qpu_id,
                                 QuantumKernel &&kernel, Args &&...args){
  return cudaq::sample_async(qpu_id, std::forward<QuantumKernel>(kernel), 
                             std::forward<Args>(args)...);
}

#if CUDAQ_USE_STD20
template <typename QuantumKernel, typename... Args>
  requires SampleCallValid<QuantumKernel, Args...>
#else
template <typename QuantumKernel, typename... Args,
          typename = std::enable_if_t<
              std::is_invocable_r_v<void, QuantumKernel, Args...>>>
#endif
async_sample_result sample_async(std::size_t shots, std::size_t qpu_id,
                                 QuantumKernel &&kernel, Args &&...args){
  return cudaq::sample_async(shots, qpu_id, std::forward<QuantumKernel>(kernel),
                             std::forward<Args>(args)...);
}

#if CUDAQ_USE_STD20
template <typename QuantumKernel, typename... Args>
  requires SampleCallValid<QuantumKernel, Args...>
#else
template <typename QuantumKernel, typename... Args,
          typename = std::enable_if_t<
              std::is_invocable_r_v<void, QuantumKernel, Args...>>>
#endif
auto sample_async(QuantumKernel &&kernel, Args &&...args){
  return cudaq::sample_async(std::forward<QuantumKernel>(kernel), 
                             std::forward<Args>(args)...);
}

#if CUDAQ_USE_STD20
template <typename QuantumKernel, typename... Args>
  requires SampleCallValid<QuantumKernel, Args...>
#else
template <typename QuantumKernel, typename... Args,
          typename = std::enable_if_t<
              std::is_invocable_r_v<void, QuantumKernel, Args...>>>
#endif
std::vector<sample_result> sample(QuantumKernel &&kernel,
                                  ArgumentSet<Args...> &&params){
  return sample(std::forward<QuantumKernel>(kernel), 
                std::forward<ArgumentSet<Args...>>(params));
}

#if CUDAQ_USE_STD20
template <typename QuantumKernel, typename... Args>
  requires SampleCallValid<QuantumKernel, Args...>
#else
template <typename QuantumKernel, typename... Args,
          typename = std::enable_if_t<
              std::is_invocable_r_v<void, QuantumKernel, Args...>>>
#endif
std::vector<sample_result> sample(std::size_t shots, QuantumKernel &&kernel,
                                  ArgumentSet<Args...> &&params){
  return cudaq::sample(shots, std::forward<QuantumKernel>(kernel), 
                       std::forward<ArgumentSet<Args...>>(params));
}

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
                                  ArgumentSet<Args...> &&params){
  return cudaq::sample(options, std::forward<QuantumKernel>(kernel),
                       std::forward<ArgumentSet<Args...>>(params));
}

}; // namespace cudaq::mqss
