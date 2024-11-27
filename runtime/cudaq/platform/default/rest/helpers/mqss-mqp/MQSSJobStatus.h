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
  
  Enum that defines the status of a job returned by the MQSS via the MQP.

*******************************************************************************  
* This source code and the accompanying materials are made available under    *
* the terms of the Apache License 2.0 which accompanies this distribution.    *
******************************************************************************/

#ifndef MQSSJOBSTATUS_H
#define MQSSJOBSTATUS_H

// Define an enumeration for JobStatus
namespace cudaq::mqss{
  enum class JobStatus {
    PENDING,
    WAITING,
    FAILED,
    COMPLETED,
    CANCELLED
  };

  const std::string jobStatusToString(JobStatus jobStatus) {
    switch (jobStatus) {
      case JobStatus::PENDING:    return "PENDING";
      case JobStatus::WAITING:    return "WAITING";
      case JobStatus::FAILED:     return "FAILED";
      case JobStatus::COMPLETED:  return "COMPLETED";
      case JobStatus::CANCELLED:  return "CANCELLED";
    }
    return "UNKNOWN";
  }
}
#endif // MQSS_JOBSTATUS_H 
