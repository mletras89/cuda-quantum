/* This code and any associated documentation is provided "as is"

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
  RabbitMQ-Consumer starts a rabbitMQ consumer waiting for queries, login,  
  send job and check the status.
  Whe processing the job, the consumer lower the quake code to QIR and the 
  invokes python scrip to simulate QIR, get results and send them back to 
  a rabbitMQ client.

*******************************************************************************
* This source code and the accompanying materials are made available under    * 
* the terms of the Apache License 2.0 which accompanies this distribution.    * 
******************************************************************************/
//#include <rabbitmq-c/amqp.h>
//#include <rabbitmq-c/framing.h>
//#include <rabbitmq-c/tcp_socket.h>
#include "RabbitMQServer.hpp"
#include <string>
#include <iostream>
#include <cstdlib>
#include <thread>
#include <vector>
#include <cstring>
#include <mutex>
#include <nlohmann/json.hpp>
#include <uuid/uuid.h>  // For generating unique correlation IDs
#include <stdexcept>
#include <unordered_map>
#include <functional>
#include <regex>
#include <fstream> 
// llvm includes
#include <llvm/Support/Base64.h>
#include "llvm/Bitcode/BitcodeReader.h"
// mlir includes
#include "mlir/IR/BuiltinOps.h"
#include "mlir/Parser/Parser.h"
#include "mlir/ExecutionEngine/OptUtils.h"
// cudaq includes
#include "common/RuntimeMLIR.h"
#include "common/JIT.h"
#include "cudaq/Optimizer/CodeGen/Pipelines.h"
#include "common/ConnectionHandler.h"

#define RABBITMQ_SERVER_ADDRESS "127.0.0.1"
#define RABBITMQ_CUDAQ_PORT     5672
#define CUDAQ_GEN_PREFIX_NAME "__nvqpp__mlirgen____"

using namespace mqss;

// Start threads to consume from each queue concurrently
std::vector<std::thread> threadsConnections;

void joinThreadsConnections() {
  for (auto &thread : threadsConnections)
    if (thread.joinable())
      thread.join();
}

// Global variables for storing created jobs and results
std::unordered_map<std::string, std::pair<std::string, std::unordered_map<int, int>>> createdJobs;
// Global variables to simulate job handling and request counting
int countJobGetRequests = 0;

std::mutex cout_mutex;

std::string generate_uuid() {
    uuid_t uuid;
    uuid_generate_random(uuid);
    char str[37];
    uuid_unparse(uuid, str);
    return std::string(str);
}

std::tuple<mlir::ModuleOp, mlir::MLIRContext *>
extractMLIRContext(const std::string& circuit){
  auto contextPtr = cudaq::initializeMLIR();
  mlir::MLIRContext &context = *contextPtr.get();

  // Get the quake representation of the kernel
  auto quakeCode = circuit;
  auto m_module = mlir::parseSourceString<mlir::ModuleOp>(quakeCode, &context);
  if (!m_module)
    throw std::runtime_error("module cannot be parsed");

  return std::make_tuple(m_module.release(), contextPtr.release());
}

// Trim leading and trailing whitespace
std::string trim(const std::string& str){
    size_t first = str.find_first_not_of(" \t\n\r\f\v"); // Find first non-whitespace
    if (first == std::string::npos) {
        return "";  // If no non-whitespace characters, return an empty string
    }
    size_t last = str.find_last_not_of(" \t\n\r\f\v");  // Find last non-whitespace
    return str.substr(first, (last - first + 1));       // Return trimmed substring
}

std::string getKernelName(const std::string& program){
  std::regex patternKernel("func\\.func @__nvqpp__mlirgen____([^\\(\\)]+)\\(\\)");
  std::smatch matches;
  if(!std::regex_search(program, matches, patternKernel))
    throw std::runtime_error("Error, no kernel function name found on the given Quake program...");
  std::string kernelName = matches[1];
  // Find the position of the substring
  size_t pos = kernelName.find(CUDAQ_GEN_PREFIX_NAME);
  // If the substring is found, erase it
  if (pos != std::string::npos) {
      kernelName.erase(pos, std::string(CUDAQ_GEN_PREFIX_NAME).length());
  }
  return trim(kernelName);
}

std::unordered_map<std::string,std::unordered_map<int, int>> parseStringToMap(const std::string &input) {
  std::unordered_map<std::string,std::unordered_map<int, int>> resultMap;
  // Regex to match outer keys and their corresponding { ... } content
  std::regex outerRegex(R"((\w+)\s*:\s*\{([^}]+)\})");
  std::smatch outerMatch;
  std::string::const_iterator searchStart(input.cbegin());
  while (std::regex_search(searchStart, input.cend(), outerMatch, outerRegex)) {
      std::string outerKey = outerMatch[1]; // Capture outer key, e.g., "__global__"
      std::string innerContent = outerMatch[2]; // Capture inner content, e.g., "0:479 1:521"
      // Parse the inner map
      std::unordered_map<int, int> innerMap;
      std::regex innerRegex(R"((\d+)\s*:\s*(\d+))");
      std::smatch innerMatch;
      std::string::const_iterator innerStart(innerContent.cbegin());
      while (std::regex_search(innerStart, innerContent.cend(), innerMatch, innerRegex)) {
          int key = std::stoi(innerMatch[1]);
          int value = std::stoi(innerMatch[2]);
          innerMap[key] = value;
          innerStart = innerMatch.suffix().first; // Move to the next match
      }
      // Add the parsed inner map to the result
      resultMap[outerKey] = innerMap;
      searchStart = outerMatch.suffix().first; // Move to the next outer match
  }
  return resultMap;
}

std::string lowerQuakeCode(const std::string &circuit, const std::string &kernelName){
  auto [m_module, contextPtr] =
      extractMLIRContext(circuit);

  mlir::MLIRContext &context = *contextPtr;
  // Extract the kernel name
  auto func = m_module.lookupSymbol<mlir::func::FuncOp>(
      std::string(CUDAQ_GEN_PREFIX_NAME + kernelName));

  auto translation = cudaq::getTranslation("qir-base");

  std::string codeStr;
  {
      llvm::raw_string_ostream outStr(codeStr);
      m_module.getContext()->disableMultithreading();
      if (failed(translation(m_module, outStr, "", false, false, false)))
        throw std::runtime_error("Could not successfully translate to qir-base");
  }

  std::vector<char> decodedBase64Output;
  // Decode the Base64 string
  if(llvm::decodeBase64(codeStr, decodedBase64Output))
    throw std::runtime_error("Error decoding Base64 string");

  std::string decodedBase64Kernel = std::string(decodedBase64Output.data(), decodedBase64Output.size());
  // decode the LLVM byte code to string
  llvm::LLVMContext contextLLVM;
  contextLLVM.setOpaquePointers(false);
  auto memoryBuffer = llvm::MemoryBuffer::getMemBuffer(decodedBase64Kernel);

  llvm::Expected<std::unique_ptr<llvm::Module>> moduleOrErr = llvm::parseBitcodeFile(*memoryBuffer, contextLLVM);
  std::error_code ec = llvm::errorToErrorCode(moduleOrErr.takeError());
  if(ec)
    throw std::runtime_error("Compiler::Error parsing bitcode..."); // when debbugin dump: ec.message())
  // Successfully parsed
  std::unique_ptr<llvm::Module> moduleConverted = std::move(*moduleOrErr);

  auto optPipeline = mlir::makeOptimizingTransformer(
      /*optLevel=*/3, /*sizeLevel=*/0,
      /*targetMachine=*/nullptr);
  if (auto err = optPipeline(moduleConverted.get()))
    throw std::runtime_error("getQIR Failed to optimize LLVM IR ");

  std::string loweredCode;
  {
    llvm::raw_string_ostream os(loweredCode);
    moduleConverted->print(os, nullptr);
  }
  return loweredCode;
}

// Function to process login
nlohmann::json processLogin(const std::string& receivedMessage){
  nlohmann::json jsonResponse = {
    {"id-token", generate_uuid().c_str()},
    {"refresh-token", "refreshtoken"},
  };
  return jsonResponse;
}

nlohmann::json getErrorAnswer(int status, const std::string& error_message){
  nlohmann::json jsonResponse = {{"status", std::to_string(status)}, {"error", error_message.c_str()}};
  return jsonResponse;
}

// Function to get job status and results
void getJobStatus(const std::string& jobId, const std::string &replyQueue,
                  const std::string &correlationId) {
  std::cout << "Running getJobStatus with id " << jobId << std::endl;
  // return job status to the rabbitmq client using the replyqueue
  RabbitMQServer replyServer(AMQP_SERVER, AMQP_PORT, QUEUE_HPC_OFFLOADER,
                             AMQP_USER, AMQP_PASSWORD);
  // Simulate asynchronous behavior by returning "running" for the first few requests
  if (countJobGetRequests < 3) {
    countJobGetRequests++;

    nlohmann::json jsonResponse = {{"status", "running"}};
    #ifdef DEBUG
    std::cout << "I am returning " << jsonResponse.dump() << std::endl;
    #endif
    replyServer.publishMessage(replyQueue, jsonResponse.dump(), correlationId, true);
    return;
  }

  // Reset the request counter after 3 "running" responses
  countJobGetRequests = 0;

  // Check if the job exists
  if (createdJobs.find(jobId) == createdJobs.end()){
    nlohmann::json errorResponse = getErrorAnswer(404,"Job not found");
    replyServer.publishMessage(replyQueue, errorResponse.dump(), correlationId, true);
    return;
  }
  // Retrieve the job data (name and counts)
  auto& [name, counts] = createdJobs[jobId];

  // Prepare the result data by expanding the counts
  std::vector<int> retData;
  for (const auto& [bits, count] : counts) {
    for (int i = 0; i < count; ++i) {
        retData.push_back(bits);
    }
  }

  // Convert the result data to a string list
  std::vector<std::string> stringResults;
  //crow::json::wvalue stringResults;
  for (int bits : retData) {
    stringResults.push_back(std::to_string(bits));
  }

  // Create the final response JSON object
  nlohmann::json resultResponse; 
  resultResponse["status"] = "completed";
  resultResponse["results"]["MOCK_SERVER_RESULTS" ] = stringResults;
  replyServer.publishMessage(replyQueue, resultResponse.dump(), correlationId, true);
}

void processJob(const std::string& receivedMessage, const std::string &replyQueue,
                const std::string &correlationId, std::string taskId){
  // return job status to the rabbitmq client using the replyqueue
  RabbitMQServer replyServer(AMQP_SERVER, AMQP_PORT, QUEUE_HPC_OFFLOADER,
                             AMQP_USER, AMQP_PASSWORD);
  nlohmann::json jobData;
  nlohmann::json errorResponse = getErrorAnswer(400,"Invalid Job Data");
  try{
    jobData = nlohmann::json::parse(receivedMessage);
  }catch(const nlohmann::json::parse_error & e){
    replyServer.publishMessage(replyQueue, errorResponse.dump(), correlationId, true);
    return;
  }

  if (!jobData.contains("name") || !jobData.contains("n_shots") || !jobData.contains("circuit_files")){
    replyServer.publishMessage(replyQueue, errorResponse.dump(), correlationId, true);
    return;
  }
  // Extract job details from the request body
  std::string jobName = jobData["name"].get<std::string>();
  int jobCount = jobData["n_shots"].get<int>();
  std::vector<std::string> circuit_files;
  auto& files = jobData["circuit_files"];
  for (size_t i = 0; i < files.size(); ++i) {
    circuit_files.push_back(files[i].get<std::string>()); // .s() gives you the string
  }
  std::string program = circuit_files[0];
  // Simulate kernel function and qubit processing
  std::string kernelName = getKernelName(program);
  std::string qirCode = lowerQuakeCode(program,kernelName);
  std::ofstream outFile(std::string("./tempCircuit.txt"), std::ios::out | std::ios::trunc);
  if (!outFile.is_open())
    throw std::runtime_error("Failed to open file!");
  outFile << qirCode;
  outFile.close();
  // Construct the system call to run Python with the input
  std::string command = std::string("python3 ./SimulateKernelLoweredToLLVM.py -c tempCircuit.txt -o tempResults.txt -s 100");
  // Execute the command
  int result = system(command.c_str());
  // Check the result (0 means success)
  if (result != 0)
    throw std::runtime_error("Python script execution failed!");
  // now read the results from file
  std::ifstream file(std::string("./tempResults.txt"));
  // Check if the file was opened successfully
  if (!file)
      throw std::runtime_error("File could not be opened!");
  // Create a stringstream object to read the file content
  std::stringstream bufferResult;
  // Read the file content into the stringstream
  bufferResult << file.rdbuf();
  // Convert the stringstream into a string
  std::string resultCircuit = bufferResult.str();
  file.close();
  std::remove("./tempCircuit.txt");
  std::remove("./tempResults.txt");
  #ifdef DEBUG
  std::cout << "Results:" << std::endl << resultCircuit << std::endl;
  #endif 
  // Generate a new UUID for the job
  std::string newJobId = generate_uuid();
  // Simulate results (in the original, this comes from some quantum function)
  std::unordered_map<std::string,std::unordered_map<int, int>> results = parseStringToMap(resultCircuit);
  // Store the created job in the global jobs dictionary
  createdJobs[newJobId] = {jobName, results[std::string("__global__")]};
  // Return the job ID as a JSON response 
  nlohmann::json resultResponse;
  resultResponse["job"] = newJobId;
  // return the job id
  replyServer.publishMessage(replyQueue, resultResponse.dump(), correlationId, true);
}

int main() {
  std::cout << "Starting rabbitmq consumer " << std::endl;
  RabbitMQServer offloaderListener(AMQP_SERVER, AMQP_PORT, QUEUE_HPC_OFFLOADER,
                                   AMQP_USER, AMQP_PASSWORD);
  // tell the offloaderListener to start to consume
  offloaderListener.startToConsume();
  while (true) {
    std::cout << "Waiting for a new job..." << std::endl;
    amqp_envelope_t envelope;
    std::string message, replyQueue, correlationId;
    offloaderListener.consumeMessage(envelope, message, replyQueue,
                                     correlationId);
    try {
    // try to parse the received message, if is a json then I have to process
    // it as a new job
      auto json = nlohmann::json::parse(message);
      std::string newTaskId = generate_uuid();
      threadsConnections.push_back(
          std::thread(processJob, message, replyQueue,
                      correlationId, newTaskId));
      std::cout << "Processing new task with id: {}" << newTaskId << std::endl;
    } catch (const nlohmann::json::parse_error& e) {
    // the message is just a regular uuid for the case when asking the status of
    // a job
      threadsConnections.push_back(std::thread(getJobStatus,
                                               message,
                                               replyQueue, correlationId));
      std::cout <<"Checking status of task with id: {}" << message << std::endl;
    }
  }
  // Ensure the logger is properly destroyed
  joinThreadsConnections();
  return 1;
}
