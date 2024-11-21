#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/framing.h>
#include <rabbitmq-c/tcp_socket.h>
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
#include "llvm/Support/Casting.h"
#include <llvm/Support/Base64.h>
#include "llvm/Bitstream/BitstreamReader.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Error.h"
#include <llvm/Support/SourceMgr.h>
#include <llvm/AsmParser/Parser.h>
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/ExecutionUtils.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
// mlir includes
#include "mlir/Target/LLVMIR/Import.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Parser/Parser.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/ImplicitLocOpBuilder.h"
#include "mlir/Pass/PassManager.h"
#include "mlir/Transforms/Passes.h"
#include "mlir/Target/LLVMIR/ModuleTranslation.h"  // For translateModuleToLLVMIR
#include "mlir/ExecutionEngine/OptUtils.h"
#include "mlir/ExecutionEngine/ExecutionEngine.h"
// cudaq includes
#include "cudaq/Frontend/nvqpp/AttributeNames.h"
#include "cudaq/Optimizer/Transforms/Passes.h"
// includes in runtime
#include "cudaq/qis/execution_manager.h"
#include "cudaq.h"
#include "common/Executor.h"
#include "common/RuntimeMLIR.h"
#include "common/Logger.h"
#include "common/JIT.h"
#include "common/ExecutionContext.h"
#include "cudaq/spin_op.h"
#include "cudaq/Optimizer/CodeGen/Pipelines.h"
#include "cudaq/Optimizer/CodeGen/Passes.h"
#include "cudaq/Optimizer/Transforms/Passes.h"
#include "cudaq/algorithm.h"

#define RABBITMQ_SERVER_ADDRESS "127.0.0.1"
#define RABBITMQ_CUDAQ_PORT     5672
#define CUDAQ_GEN_PREFIX_NAME "__nvqpp__mlirgen____"
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
  assert(std::regex_search(program, matches, patternKernel) &&
         "Error, no kernel function name found on the given Quake program..." );
  std::string kernelName = matches[1];  // The key inside curly braces starting with __nvqpp__mlirgen____
  // Find the position of the substring
  size_t pos = kernelName.find(CUDAQ_GEN_PREFIX_NAME);
  // If the substring is found, erase it
  if (pos != std::string::npos) {
      kernelName.erase(pos, std::string(CUDAQ_GEN_PREFIX_NAME).length());
  }

  return trim(kernelName);
}

std::unordered_map<int, int> parseStringToMap(const std::string &input) {
  std::unordered_map<int, int> resultMap;
  std::string cleanedInput = input;

  // Remove curly braces '{' and '}'
  cleanedInput.erase(std::remove(cleanedInput.begin(), cleanedInput.end(), '{'), cleanedInput.end());
  cleanedInput.erase(std::remove(cleanedInput.begin(), cleanedInput.end(), '}'), cleanedInput.end());

  std::stringstream ss(cleanedInput);
  std::string keyValuePair;

  // Read each key-value pair
  while (ss >> keyValuePair) {
      size_t colonPos = keyValuePair.find(':');

      if (colonPos != std::string::npos) {
          // Get the key (left of colon) and value (right of colon)
          int key   = std::stoi(keyValuePair.substr(0, colonPos));
          int value = std::stoi(keyValuePair.substr(colonPos + 1));

          // Insert into map
          resultMap[key] = value;
      }
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
  assert (!llvm::decodeBase64(codeStr, decodedBase64Output) &&
          "Error decoding Base64 string");
  std::string decodedBase64Kernel = std::string(decodedBase64Output.data(), decodedBase64Output.size());
  // decode the LLVM byte code to string
  llvm::LLVMContext contextLLVM;
  contextLLVM.setOpaquePointers(false);
  auto memoryBuffer = llvm::MemoryBuffer::getMemBuffer(decodedBase64Kernel);

  llvm::Expected<std::unique_ptr<llvm::Module>> moduleOrErr = llvm::parseBitcodeFile(*memoryBuffer, contextLLVM);
  std::error_code ec = llvm::errorToErrorCode(moduleOrErr.takeError());
  assert(!ec && "Compiler::Error parsing bitcode..."); // when debbugin dump: ec.message())

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
nlohmann::json getJobStatus(const std::string& jobId) {
  std::cout << "Runninbg getJobStatus with id " << jobId << std::endl;
  // Simulate asynchronous behavior by returning "running" for the first few requests
  if (countJobGetRequests < 3) {
    countJobGetRequests++;

    nlohmann::json jsonResponse = {{"status", "running"}};
    std::cout << "I am returning " << jsonResponse.dump() << std::endl;
    return jsonResponse;
  }

  // Reset the request counter after 3 "running" responses
  countJobGetRequests = 0;

  // Check if the job exists
  if (createdJobs.find(jobId) == createdJobs.end())
    return getErrorAnswer(404,"Job not found");

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
  return resultResponse;
}

nlohmann::json processJob(const std::string& receivedMessage){
  nlohmann::json jobData;
  try{
    jobData = nlohmann::json::parse(receivedMessage);
  }catch(const nlohmann::json::parse_error & e){
    return getErrorAnswer(400,"Invalid Job Data");
  }

  if (!jobData.contains("name") || !jobData.contains("count") || !jobData.contains("program")) 
    return getErrorAnswer(400,"Invalid Job Data");

  // Extract job details from the request body
  std::string jobName = jobData["name"].get<std::string>();
  int jobCount = jobData["count"].get<int>();
  std::string program = jobData["program"].get<std::string>();
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
  std::cout << "Results:" << std::endl << resultCircuit << std::endl;
  // Generate a new UUID for the job
  std::string newJobId = generate_uuid();
  // Simulate results (in the original, this comes from some quantum function)
  std::unordered_map<int, int> results = parseStringToMap(resultCircuit);
  // Store the created job in the global jobs dictionary
  createdJobs[newJobId] = {jobName, results};
 
  nlohmann::json resultResponse;
  resultResponse["job"] = newJobId;
 
  return resultResponse;
}

amqp_connection_state_t setupConnection(const std::string& queue_name){
  // Create a new RabbitMQ connection for each thread
  amqp_connection_state_t conn = amqp_new_connection();
  amqp_socket_t* socket = amqp_tcp_socket_new(conn);
  if (!socket)
    throw std::runtime_error("Failed to create TCP socket for "+queue_name);
  
  // Connect to RabbitMQ server
  if (amqp_socket_open(socket, RABBITMQ_SERVER_ADDRESS, RABBITMQ_CUDAQ_PORT)) 
    throw std::runtime_error("Failed to open TCP socket for "+queue_name);
  
  // Log in to the RabbitMQ server
  amqp_rpc_reply_t login_reply = amqp_login(conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, 
                                            "guest", "guest");
  if (login_reply.reply_type != AMQP_RESPONSE_NORMAL)
    throw std::runtime_error("Login failed for "+queue_name);
  
  // Open a channel
  amqp_channel_open(conn, 1);
  amqp_rpc_reply_t channel_reply = amqp_get_rpc_reply(conn);
  if (channel_reply.reply_type != AMQP_RESPONSE_NORMAL)
    throw std::runtime_error("Opening channel failed for "+queue_name);

  // Declare the queue
  amqp_queue_declare(conn, 1, amqp_cstring_bytes(queue_name.c_str()), 
                      0, 1, 0, 0, amqp_empty_table);
  amqp_get_rpc_reply(conn);
  
  // Start consuming messages from the queue
  amqp_basic_consume(conn, 1, amqp_cstring_bytes(queue_name.c_str()), amqp_empty_bytes, 
                      0, 1, 0, amqp_empty_table);
  amqp_get_rpc_reply(conn);

  return conn;
}

void closeConnection(const amqp_connection_state_t& conn){
  // Close the channel and connection
  amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS);
  amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
  amqp_destroy_connection(conn);
}

// readRequest, receives as parameter the name of the queue 
// and returns the request

void readRequestFromQueue(const amqp_connection_state_t& conn, 
                          const std::string& queue_name, 
                          amqp_envelope_t& envelope, std::string& message,
                          std::string& reply_to, std::string& correlation_id){
  amqp_rpc_reply_t res;
  
  // Attempt to get the next message from the queue
  res = amqp_consume_message(conn, &envelope, NULL, 0);
  
  if (res.reply_type != AMQP_RESPONSE_NORMAL)
    throw std::runtime_error("Error consuming message from "+queue_name);
  
  reply_to = std::string((char*)envelope.message.properties.reply_to.bytes, envelope.message.properties.reply_to.len);
  correlation_id = std::string((char*)envelope.message.properties.correlation_id.bytes, envelope.message.properties.correlation_id.len);
  // Retrieve and process message
  message = std::string((char*)envelope.message.body.bytes, envelope.message.body.len);
}

void answerRequestToQueue(const amqp_connection_state_t& conn,
                          const std::string& reply_to,
                          const std::string& message,
                          const std::string& correlation_id,        
                          bool isJSON){

  amqp_basic_properties_t props;
  props._flags = AMQP_BASIC_CONTENT_TYPE_FLAG | AMQP_BASIC_CORRELATION_ID_FLAG;
  props.content_type = amqp_cstring_bytes("text/plain");
  if (isJSON)
    props.content_type = amqp_cstring_bytes("application/json");
  std::cout << "Answer correlation id: "<< correlation_id << std::endl;
  props.correlation_id = amqp_cstring_bytes(correlation_id.c_str());
  // sending the response
  amqp_basic_publish(conn, 1, amqp_empty_bytes, amqp_cstring_bytes(reply_to.c_str()), 
                      0, 0, &props, amqp_cstring_bytes(message.c_str()));
}

void processConnection(std::function<nlohmann::json(std::string)> function, 
                       const std::string& queue_name) {
  std::cout << "Started consuming messages from " << queue_name << std::endl;
  amqp_connection_state_t conn = setupConnection(queue_name);
  // Consume loop
  while (true) {
    amqp_envelope_t envelope;
    std::string message, correlation_id, reply_to;
    readRequestFromQueue(conn,queue_name,envelope,message,reply_to,correlation_id);
    std::cout << "Process from Queue "<< queue_name << std::endl;
    std::cout << "Received Message: " << message << std::endl;
    std::cout << "Correlation id: " << correlation_id << std::endl;
    std::cout << "Reply to: " << reply_to << std::endl;
    // function has to return json
    auto jsonResponse = function(message);
    std::string json_str = jsonResponse.dump();
    answerRequestToQueue(conn, reply_to, json_str, correlation_id, true);
    amqp_destroy_envelope(&envelope);
  }
  std::cout << "Stopped consuming from " << queue_name << std::endl;
}

int main() {
  std::cout << "Starting rabbitmq consumer " << std::endl;
  // Define the queue names
  const std::string queueLogin      = "/login";
  const std::string queueJob        = "/job";
  const std::string queueJobString  = "/job/<string>";
  
  // Start threads to consume from each queue concurrently
  std::vector<std::thread> threads;
  
  // Creating a thread for each queue
  threads.push_back(std::thread(processConnection, processLogin, queueLogin));
  threads.push_back(std::thread(processConnection, processJob, queueJob));
  threads.push_back(std::thread(processConnection, getJobStatus, queueJobString));
  
  // Wait for all threads to finish
  for (auto& t : threads) {
      t.join();
  }
  
  return 0;
}
