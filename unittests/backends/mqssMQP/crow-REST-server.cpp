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
  @date   September 2024
  @version 1.0
  @ brief
	Fake server used to test the connection of CudaQ and the MQSS.
	This file is a cpp version of the fake server provided by Nvidia.
	The server runs on "0.0.0.0" on port 62440.
	The HTTP server responds to the following queries:
		- "/login" just a normal login
		- "/job" this creates a job in the server, as described in the
		  Struct Job. The server receives a JSON string containing the
		  name, count (shots) and the program (quake). It returns a job ID
		  used latter to veriy the job status or to obtaine the results.
		- "/job/<string>" this receives the job ID. If the job is still
		  running, it will return {"status", "running"}. Once the job is
		  done, it returns {"status", "running","results",
                  {MOCK_SERVER_RESULTS, ACTUAL RESULTS}}.

 *******************************************************************************
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/

#include <crow.h>  // Include Crow header file
#include <unordered_map>
#include <uuid/uuid.h>  // For generating UUIDs
#include <regex>
#include <string>
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

#define CUDAQ_GEN_PREFIX_NAME "__nvqpp__mlirgen____"

// Define the Job structure to handle the incoming job data
struct Job {
    std::string name;
    int count;
    std::string program;
};

// Global variables for storing created jobs and results
std::unordered_map<std::string, std::pair<std::string, std::unordered_map<int, int>>> createdJobs;
// Global variables to simulate job handling and request counting
int countJobGetRequests = 0;
// Function to generate a new UUID as a string
std::string generateUUID() {
    uuid_t uuid;
    uuid_generate_random(uuid);  // Generate a random UUID
    char uuidStr[37];            // UUIDs are 36 characters plus null terminator
    uuid_unparse(uuid, uuidStr);  // Convert UUID to string
    return std::string(uuidStr);
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

// Function to get job status and results
crow::response getJobStatus(const std::string& jobId) {
    // Simulate asynchronous behavior by returning "running" for the first few requests
    if (countJobGetRequests < 3) {
        countJobGetRequests++;
        return crow::response(crow::json::wvalue{{"status", "running"}});
    }

    // Reset the request counter after 3 "running" responses
    countJobGetRequests = 0;

    // Check if the job exists
    if (createdJobs.find(jobId) == createdJobs.end()) {
        return crow::response(404, "Job not found");
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
    crow::json::wvalue resultResponse;

    resultResponse["status"] = "completed";
    resultResponse["results"]["MOCK_SERVER_RESULTS" ] = stringResults;
    return crow::response(resultResponse);
}

void startServer(int port) {
    crow::SimpleApp app;  // Create a Crow HTTP application

    // POST request for login: Check the "Authorization" header
    CROW_ROUTE(app, "/login").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) {
        // Extract the Authorization header from the request
        auto auth_header = req.get_header_value("Authorization");

        // If the Authorization header is missing or empty, return a 401 Unauthorized
        if (auth_header.empty()) {
            crow::json::wvalue error_response;
            error_response["detail"] = "Credentials not provided";
            return crow::response(401, error_response);
        }

        // If the Authorization header is present, return tokens
        crow::json::wvalue result;
        result["id-token"] = "hello";            // Sample ID token
        result["refresh-token"] = "refreshToken";  // Sample refresh token

        return crow::response(result);  // Return tokens in JSON format
    });

    // POST request for /job: Posting a new job
    CROW_ROUTE(app, "/job").methods(crow::HTTPMethod::POST)
    ([](const crow::request& req) {
        // Get the Authorization header
        auto authHeader = req.get_header_value("Authorization");

        // Check if the Authorization header is provided, if not return 401
        if (authHeader.empty()) {
            crow::json::wvalue error_response;
            error_response["detail"] = "Credentials not provided";
            return crow::response(401, error_response);
        }

        // Parse the incoming JSON data for the job
        auto jobData = crow::json::load(req.body);
        if (!jobData || !jobData.has("name") || !jobData.has("count") || !jobData.has("program")) {
            return crow::response(400, "Invalid Job Data");
        }
        // Extract job details from the request body
        std::string jobName = jobData["name"].s();
        int jobCount = jobData["count"].i();
        std::string program = jobData["program"].s();

        // Log job information
        /*std::cout << "Posting job with name = " << jobName << ", count = " << jobCount << std::endl;
        std::cout << "Quake " <<std::endl << program << std::endl;*/
        std::string kernelName = getKernelName(program);
        //std::cout << "Kernel Name: " << kernelName << std::endl;

        std::string qirCode = lowerQuakeCode(program,kernelName);
        //std::cout << "QIR: " << std::endl << qirCode << std::endl; 

        std::ofstream outFile(std::string("./tempCircuit.txt"), std::ios::out | std::ios::trunc);

        if (!outFile.is_open())
          throw std::runtime_error("Failed to open file!");

        outFile << qirCode;
        outFile.close();

        // Construct the system call to run Python with the input
        std::string command = std::string("python3 ./SimulateKernelLoweredToLLVM.py -c tempCircuit.txt -o tempResults.txt -s 1000");
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
        //std::cout << "Results:" << std::endl << resultCircuit << std::endl;
        // Generate a new UUID for the job
        std::string newJobId = generateUUID();
        // Simulate results (in the original, this comes from some quantum function)
        std::unordered_map<std::string,std::unordered_map<int, int>> results = parseStringToMap(resultCircuit);
        // Store the created job in the global jobs dictionary
        createdJobs[newJobId] = {jobName, results[std::string("__global__")]};

        // Return the job ID as a JSON response
        crow::json::wvalue result_response;
        result_response["job"] = newJobId;
        return crow::response(result_response);
    });

   // Define the GET route for retrieving job status and results
    CROW_ROUTE(app, "/job/<string>").methods(crow::HTTPMethod::GET)
    ([](const crow::request& req, std::string jobId) {
        return getJobStatus(jobId);
    });

    // Start the server
    app.bindaddr("0.0.0.0").port(port).multithreaded().run();
}

int main() {
    startServer(62440);
    return 0;
}
