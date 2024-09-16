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

std::string mockPort = "62440";
std::string backendStringTemplate = "mqss;emulate;false;url;http://localhost:{};credentials;{}";

bool isValidExpVal(double value) {
  // give us some wiggle room while keep the tests fast
  return value < -1.1 && value > -2.3;
}

CUDAQ_TEST(MQSSTester, checkSampleSync) {
  std::string home = std::getenv("HOME");
  std::string fileName = home + "/FakeCppMQSS.config";
  auto backendString =
      fmt::format(fmt::runtime(backendStringTemplate), mockPort, fileName);
  std::cout << "backendString:: " << backendString << std::endl;

  auto &platform = cudaq::get_platform();
  platform.setTargetBackend(backendString);
  //std::cout << "platform.name():" << platform.name() << std::endl;
  auto kernel = cudaq::make_kernel();

  auto qubit = kernel.qalloc(2);
  kernel.h(qubit[0]);
  kernel.mz(qubit[0]);
  auto name = cudaq::getKernelName(kernel);
  //auto quakeCode = cudaq::get_quake_by_name(kernel.name()); //, false);
  //std::cout << "INFO OF KERNEL: KERNEL NAME = " << kernel.name() << " second name "<< name <<" QUAKE CODE = " << kernel.to_quake() << std::endl;
  auto counts = cudaq::sample(kernel);
  counts.dump();
  EXPECT_EQ(counts.size(), 2);
}

int main(int argc, char **argv) {
  std::string home = std::getenv("HOME");
  std::string fileName = home + "/FakeCppMQSS.config";
  std::ofstream out(fileName);
  out << "key: key\nrefresh: refresh\ntime: 0";
  out.close();
  ::testing::InitGoogleTest(&argc, argv);
  auto ret = RUN_ALL_TESTS();
  std::remove(fileName.c_str());
  return ret;
}
