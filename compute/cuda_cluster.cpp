#include "cuda_cluster.h"

#include <cuda.h>
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <nvrtc.h>

using namespace alt;
using namespace compute;

static deviceInfo makeCudaDeviceInfo(cudaDeviceProp &prop, int index)
{
    deviceInfo inf;

    inf.name = string::fromFix(prop.name,sizeof(prop.name));
    inf.index = index;
    inf.api_type = "cuda";
    inf.api_version = string::fromInt(prop.major)+"."+string::fromInt(prop.minor);

    inf.max_threads_per_dimension = {prop.maxThreadsDim[0],prop.maxThreadsDim[1],prop.maxThreadsDim[2]};
    inf.max_threads_total = prop.maxThreadsPerBlock;
    inf.max_grid_size = {prop.maxGridSize[0],prop.maxGridSize[1],prop.maxGridSize[2]};

    inf.global_memory_size = prop.totalGlobalMem;
    inf.shared_memory_size = imath::max(prop.sharedMemPerBlockOptin,prop.sharedMemPerBlock);
    inf.constant_memory_size = prop.totalConstMem;
    inf.max_warp_size = prop.warpSize;
    inf.total_cores = prop.multiProcessorCount;

    inf.api_related["asyncEngineCount"] = prop.asyncEngineCount;
    inf.api_related["unifiedAddressing"] = prop.unifiedAddressing;
    inf.api_related["canMapHostMemory"] = prop.canMapHostMemory;
    inf.api_related["concurrentKernels"] = prop.concurrentKernels;
    inf.api_related["hostNativeAtomicSupported"] = prop.hostNativeAtomicSupported;
    inf.api_related["canUseHostPointerForRegisteredMem"] = prop.canUseHostPointerForRegisteredMem;
    inf.api_related["directManagedMemAccessFromHost"] = prop.directManagedMemAccessFromHost;

    return inf;
}

retCode cudaCluster::cudaErrorFilter(int err)
{
    //todo: закончить и сделать более упорядоченным

    if(cudaSuccess == err)
        return 0;

    switch(err)
    {
    case cudaErrorLaunchFileScopedSurf:
    case cudaErrorLaunchFileScopedTex:
    case cudaErrorMissingConfiguration:
    case cudaErrorInvalidSurface:
    case cudaErrorInvalidNormSetting:
    case cudaErrorInvalidFilterSetting:
    case cudaErrorInvalidMemcpyDirection:
    case cudaErrorInvalidChannelDescriptor:
    case cudaErrorInvalidTextureBinding:
    case cudaErrorInvalidTexture:
    case cudaErrorInvalidHostPointer:
    case cudaErrorInvalidDevicePointer:
    case cudaErrorInvalidPitchValue:
    case cudaErrorInvalidValue:
        last_error = errorCodeInvalidValue; break;


    case cudaErrorDuplicateSurfaceName:
    case cudaErrorDuplicateTextureName:
    case cudaErrorDuplicateVariableName:
    case cudaErrorInvalidSymbol:
        last_error = errorCodeInvalidSymbol; break;

    case cudaErrorMemoryAllocation: last_error = errorCodeMemoryAllocation; break;

    case cudaErrorStartupFailure:
    case cudaErrorSoftwareValidityNotEstablished:
    case cudaErrorDeviceNotLicensed:
    case cudaErrorInvalidDevice:
    case cudaErrorInvalidDeviceFunction:
    case cudaErrorNoDevice:
    case cudaErrorIncompatibleDriverContext:
    case cudaErrorCallRequiresNewerDriver:
    case cudaErrorInsufficientDriver:
    case cudaErrorStubLibrary:
    case cudaErrorInitializationError:
        last_error = errorCodeInitFail; break;

    case cudaErrorProfilerDisabled: last_error = errorCodeProfilerDisabled; break;
    case cudaErrorCudartUnloading: last_error = errorCodeWrongDestruction; break;

    case cudaErrorLaunchPendingCountExceeded:
    case cudaErrorSyncDepthExceeded:
    case cudaErrorLaunchMaxDepthExceeded:
    case cudaErrorInvalidConfiguration:
        last_error = errorCoderOutOfResources; break;

    case cudaErrorDevicesUnavailable:
         last_error = errorCodeDeviceUnavailable; break;

    case cudaErrorProfilerNotInitialized:
    case cudaErrorProfilerAlreadyStarted:
    case cudaErrorProfilerAlreadyStopped:
        break;

    default: last_error = -1; break;
    };

    printLog("Cuda error: " + string::fromInt(err)+"\n");

    return last_error;
}

cudaCluster::cudaCluster()
    : computeCluster()
{
    int count;
    if(cudaErrorFilter(cudaGetDeviceCount(&count)).error())
    {
        last_error = errorCodeInitFail;
        return;
    }

    for(int i=0;i<count;i++)
    {
        cudaDeviceProp prop;
        if(cudaErrorFilter(cudaGetDeviceProperties(&prop, i)).error())
            continue;

        deviceInfo inf = makeCudaDeviceInfo(prop,i);
        device_info.append(inf);
    }

    if(!device_info.size())
    {
        last_error = errorCodeInitFail;
        return;
    }

    cudaErrorFilter(cudaSetDevice(device_info[0].index));
}

cudaCluster::~cudaCluster()
{

}

retCode cudaCluster::sync(int device, int queue)
{

    return 0;
}

retCode cudaCluster::initExtraQueues(int device, int count)
{
    return 0;
}

retCode cudaCluster::attachDevice(int index)
{
    if(device_info.size()<=index)
    {
        last_error = errorCodeInitFail;
        return last_error;
    }

    return cudaErrorFilter(cudaSetDevice(device_info[index].index));
}

computeKernel cudaCluster::kernelCompile(int device, const string &name, const string &code, const string &options)
{
    computeKernel rv;


    return rv;
}

retCode cudaCluster::kernelCall(const computeKernel &hand, dimensions<uint64> threads_block, dimensions<uint64> works_area)
{
    return 0;
}

retCode cudaCluster::kernelConfig(computeKernel &hand, const string &symbol, const byteArray &data)
{
    return 0;
}

retCode cudaCluster::kernelDestroy(computeKernel &hand)
{
    return 0;
}

computeMemory cudaCluster::memAlloc(int device, uint64 size, void *host_data, uint64 host_size, int queue, void *callback_user_data)
{
    computeMemory rv;

    return rv;
}

retCode cudaCluster::memFree(computeMemory &hand)
{
    return 0;
}

retCode cudaCluster::memHostRead(void *host_dst, computeMemory &src, uint64 size, uint64 src_offset, int queue, void *callback_user_data)
{
    return 0;
}

retCode cudaCluster::memHostWrite(computeMemory &dst, void *host_src, uint64 size, uint64 dst_offset, uint64 host_size, int queue, void *callback_user_data)
{
    return 0;
}

retCode cudaCluster::memCopy(computeMemory &dst, computeMemory &src, uint64 size, uint64 dst_offset, uint64 src_offset, int dst_queue, int src_queue, void *callback_user_data)
{
    return 0;
}
