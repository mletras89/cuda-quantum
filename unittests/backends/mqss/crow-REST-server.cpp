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

#include "crow.h"  // Include Crow header file
#include <unordered_map>
#include <uuid/uuid.h>  // For generating UUIDs

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
        std::cout << "Posting job with name = " << jobName << ", count = " << jobCount << std::endl;
        std::cout << "Quake " << program << std::endl;

        // Generate a new UUID for the job
        std::string newJobId = generateUUID();

        // Simulate kernel function and qubit processing
        std::string kernelFunctionName = "testQuakeFunction";
        int numQubitsRequired = 2;

        // Log kernel and qubit details
        std::cout << "Kernel name = " << kernelFunctionName << std::endl;
        std::cout << "Requires " << numQubitsRequired << " qubits" << std::endl;

        // Simulate results (in the original, this comes from some quantum function)
        std::unordered_map<int, int> results;
        results[0] = 499;
        results[1] = 501;

        // Store the created job in the global jobs dictionary
        createdJobs[newJobId] = {jobName, results};

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
