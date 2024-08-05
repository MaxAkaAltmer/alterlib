#include "compute_cluster.h"

#include <iostream>

using namespace alt;
using namespace compute;

computeCluster::computeCluster()
{

}

void computeCluster::printLog(string text)
{
    std::cout << text();
}

string computeCluster::retCodeToString(retCode code)
{
    if(code.error())
    {
        switch(code)
        {
        case errorCodeInitFail:
            return "Error: Device or API initalization failed or incorrect driver of device.";
        case errorCodeInvalidValue:
            return "Error: One or more of the parameters passed to the API call is not within an acceptable range of values.";
        case errorCodeMemoryAllocation:
            return "Error: Unable to allocate enough memory to perform the requested operation.";
        case errorCodeProfilerDisabled:
            return "Error: Profiler is not initialized for this run.";
        case errorCodeWrongDestruction:
            return "Error: Wrong destruction order related to API.";
        case errorCoderOutOfResources:
            return "Error: Kernel launch is requesting resources that can never be satisfied by the current device.";
        case errorCodeInvalidSymbol:
            return "Error: Symbol name/identifier passed to the API cal is not a valid name or identifier or has name conflicts.";
        case errorCodeDeviceUnavailable:
            return "Error: Device is busy or unavailable at the current time.";

        }
        return "Error: Undefined code "+string::fromInt(code.error())+".";
    }
    else if(code.value())
    {
        return "Unknown result code "+string::fromInt(code.value())+".";
    }
    return "Ok!";
}
