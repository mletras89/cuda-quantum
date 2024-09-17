# ============================================================================ #
# Copyright (c) 2022 - 2024 NVIDIA Corporation & Affiliates.                   #
# All rights reserved.                                                         #
#                                                                              #
# This source code and the accompanying materials are made available under     #
# the terms of the Apache License 2.0 which accompanies this distribution.     #
# ============================================================================ #

import cudaq
from fastapi import FastAPI, HTTPException, Header
from typing import Union
import uvicorn, uuid, base64, ctypes
from pydantic import BaseModel
from llvmlite import binding as llvm

# Define the REST Server App
app = FastAPI()


# Jobs look like the following type
class Job(BaseModel):
    name: str
    program: str
    count: int

# Keep track of Job Ids to their Names
createdJobs = {}

# Could how many times the client has requested the Job
countJobGetRequests = 0

# At the moment, the test is not lowering the quake code to 
# LLVM. At the moment, it only returns fake results.
# Maybe in the future, we will simulate the quake code to have
# more tests

#llvm.initialize()
#llvm.initialize_native_target()
#llvm.initialize_native_asmprinter()
#target = llvm.Target.from_default_triple()
#targetMachine = target.create_target_machine()
#backing_mod = llvm.parse_assembly("")
#engine = llvm.create_mcjit_compiler(backing_mod, targetMachine)

def getKernelFunction(module):
    for f in module.functions:
        if not f.is_declaration:
            return f
    return None

def getNumRequiredQubits(function):
    for a in function.attributes:
        if "requiredQubits" in str(a):
            return int(
                str(a).split("requiredQubits\"=")[-1].split(" ")[0].replace(
                    "\"", ""))

# Here we test that the login endpoint works
@app.post("/login")
async def login(token: Union[str, None] = Header(alias="Authorization",
                                                 default=None)):
    if 'token' == None:
        raise HTTPException(status_code(401), detail="Credentials not provided")
    return {"id-token": "hello", "refresh-token": "refreshToken"}

# Here we expose a way to post jobs,
# Must have a Access Token, Job Program must be Quake
@app.post("/job")
async def postJob(job: Job,
                  token: Union[str, None] = Header(alias="Authorization",
                                                   default=None)):
    global createdJobs, shots

    if 'token' == None:
        raise HTTPException(status_code(401), detail="Credentials not provided")

    print('Posting job with name = ', job.name, job.count)
    name = job.name
    newId = str(uuid.uuid4())
    program = job.program
    print("Quake "+program)

    # At the moment the tests is hardcoded, reads results and return fake results
    kernelFunctionName = "testQuakeFunction"
    numQubitsRequired = 2 

    print("Kernel name = ", kernelFunctionName)
    print("Requires {} qubits".format(numQubitsRequired))
    #print("newId {}".format(newId))

    # Fake resuls
    results = {}
    results[0] = 499
    results[1] = 501

    createdJobs[newId] = (name, results)
    # Job "created", return the id
    return {"job": newId}


# Retrieve the job, simulate having to wait by counting to 3
# until we return the job results
@app.get("/job/{jobId}")
async def getJob(jobId: str):
    #print("Entering to the get")
    global countJobGetRequests, createdJobs, shots

    # Simulate asynchronous execution
    if countJobGetRequests < 3:
        countJobGetRequests += 1
        return {"status": "running"}

    countJobGetRequests = 0
    name, counts = createdJobs[jobId]

    retData = []
    for bits, count in counts.items():
        retData += [bits] * count

    # The simulators don't implement result recording features yet, so we have
    # to mark these results specially (MOCK_SERVER_RESULTS) in order to allow
    # downstream code to recognize that this isn't from a true Quantinuum QPU.
    list_str = map(str, retData)
    res = {"status": "completed", "results": {"MOCK_SERVER_RESULTS": list(list_str)}}
    return res

def startServer(port):
    uvicorn.run(app, port=port, host='0.0.0.0', log_level="info")

if __name__ == '__main__':
    startServer(62440)
