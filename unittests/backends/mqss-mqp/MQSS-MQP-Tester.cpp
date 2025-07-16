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
	Test to verify the connection of CudaQ and the MQSS. The test is mainly
	based on Quantinuum test provided by Nvidia.
	When executing the cudaq::sample(kernel), the runtime manager of CudaQ
	sends a job containing the quake code that has to be processed by the
	MQSS. Once the job is done, MQSS shall return the results. The
	cudaq::sample returns the results.

 *******************************************************************************
 * This source code and the accompanying materials are made available under    *
 * the terms of the Apache License 2.0 which accompanies this distribution.    *
 ******************************************************************************/

#include "CUDAQTestUtils.h"
#include "common/FmtCore.h"
#include "cudaq/algorithm.h"
#include <fstream>
#include <gtest/gtest.h>
#include <regex>

std::string backendString = "mqssMQP;emulate;false";

CUDAQ_TEST(MQSSTester, checkSampleSync) {
  auto &platform = cudaq::get_platform();
  platform.setTargetBackend(backendString);

  // create a simple circuit
  auto kernel = cudaq::make_kernel();
  auto qubit = kernel.qalloc(2);
  kernel.h(qubit[0]);
  kernel.x(qubit[1]);
  kernel.mz(qubit);
  
  // execute the circuit
  auto counts = cudaq::sample(kernel);
  counts.dump();
  // Check results
  EXPECT_EQ(counts.size(), 2);
}

int main(int argc, char **argv) {
  if (setenv("MQSS_MQP_TOKEN", "/home/token.txt", 1) != 0) {
    std::cerr << "Failed to set environment variable MQSS_MQP_TOKEN.\n";
    return 1;
  }
  if (setenv("MQSS_MQP_SERVER_URL", "http://localhost:8000/", 1) != 0) {
    std::cerr << "Failed to set environment variable MQSS_MQP_SERVER_URL.\n";
    return 1;
  }
  ::testing::InitGoogleTest(&argc, argv);
  auto ret = RUN_ALL_TESTS();
  return ret;
}
