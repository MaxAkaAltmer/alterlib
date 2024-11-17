#include "cuda_cluster.h"

#include <cuda.h>
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <nvrtc.h>

using namespace alt;
using namespace compute;

static retCode cudaTranslateError(int err)
{
    //todo: finish and make nore ordered

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
        return errorCodeInvalidValue;


    case cudaErrorDuplicateSurfaceName:
    case cudaErrorDuplicateTextureName:
    case cudaErrorDuplicateVariableName:
    case cudaErrorInvalidSymbol:
        return errorCodeInvalidSymbol;

    case cudaErrorMemoryAllocation:
        return errorCodeMemoryAllocation;

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
        return errorCodeInitFail;

    case cudaErrorProfilerDisabled:
        return errorCodeProfilerDisabled;
    case cudaErrorCudartUnloading:
        return errorCodeWrongDestruction;

    case cudaErrorLaunchPendingCountExceeded:
    case cudaErrorSyncDepthExceeded:
    case cudaErrorLaunchMaxDepthExceeded:
    case cudaErrorInvalidConfiguration:
        return errorCoderOutOfResources;

    case cudaErrorDevicesUnavailable:
         return errorCodeDeviceUnavailable;

    case cudaErrorProfilerNotInitialized:
    case cudaErrorProfilerAlreadyStarted:
    case cudaErrorProfilerAlreadyStopped:
        return 0;

    default: break;
    };

    return -1;
}

struct cudaStuff
{
    hash<int,array<cudaStream_t>> streams;
};

struct kernelStuff
{
    string name;
    string code;
    string arch;

    CUmodule module = nullptr;
    CUfunction kernel = nullptr;
};

struct callbackData
{
    void *user_data;
    cudaCluster *cluster;
    int device;
    int queue;

    delegateProc<retCode,void*/*callback_user_data*/,computeCluster*,int/*device*/,int/*queue*/> callback;
};

static void CUDART_CB streamCallback(cudaStream_t stream, cudaError_t status, void *userData)
{
    callbackData *d = reinterpret_cast<callbackData*>(userData);
    d->callback(cudaTranslateError(status),d->user_data,d->cluster,d->device,d->queue);
    delete d;
}

retCode cudaCluster::cudaErrorFilter(int err)
{
    if(err != cudaSuccess)
        printLog("Cuda error: " + string::fromInt(err)+"\n");

    return setError(cudaTranslateError(err));
}

static deviceInfo makeCudaDeviceInfo(cudaDeviceProp &prop, int index)
{
    deviceInfo inf;

    inf.name = string::fromFix(prop.name,sizeof(prop.name));
    inf.index = index;
    inf.api_type = "cuda";
    inf.api_version = string::fromInt(prop.major)+string::fromInt(prop.minor);

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
    inf.api_related["computeMode"] = prop.computeMode;

    return inf;
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

    attachDevice(0);

    internal_data = new cudaStuff;
}

cudaCluster::~cudaCluster()
{
    cudaStuff *stuff = reinterpret_cast<cudaStuff*>(internal_data);

    for(int d=0;d<stuff->streams.size();d++)
    {
        int device = stuff->streams.key(d);
        attachDevice(device);

        for(int i=0; i<stuff->streams[device].size(); i++)
        {
            cudaStreamDestroy(stuff->streams[device][i]);
        }
    }

    delete stuff;
}

retCode cudaCluster::sync(int device, int queue)
{
    if(last_error.error()) return last_error;
    cudaStuff *stuff = reinterpret_cast<cudaStuff*>(internal_data);

    attachDevice(device);
    if(queue<=0 || !stuff->streams.contains(device) || stuff->streams[device].size()<queue)
        cudaStreamSynchronize(nullptr);
    else
        cudaStreamSynchronize(stuff->streams[device][queue-1]);

    return 0;
}

retCode cudaCluster::initExtraQueues(int device, int count)
{
    if(last_error.error()) return last_error;
    cudaStuff *stuff = reinterpret_cast<cudaStuff*>(internal_data);

    attachDevice(device);

    int curr_size = stuff->streams[device].size();

    if(curr_size == count)
        return 0;

    if(curr_size < count)
    {
        stuff->streams[device].resize(count);
        for(int i=curr_size;i<count;i++)
        {
            cudaStreamCreate(&stuff->streams[device][i]);
        }
    }

    if(curr_size > count)
    {
        for(int i=count;i<curr_size;i++)
        {
            cudaStreamDestroy(stuff->streams[device][i]);
        }
        stuff->streams[device].resize(count);
    }

    return 0;
}

retCode cudaCluster::attachDevice(int index)
{
    if(last_error.error()) return last_error;

    if(device_info.size()<=index)
    {
        last_error = errorCodeInitFail;
        return last_error;
    }

    return cudaErrorFilter(cudaSetDevice(device_info[index].index));
}

//todo: error traqnslations for other APIs
computeKernel cudaCluster::kernelCompile(int device, const string &name, const string &code, const compileOptions *opt)
{
    computeKernel rv;
    if(last_error.error()) return rv;
    cudaStuff *stuff = reinterpret_cast<cudaStuff*>(internal_data);

    if(attachDevice(device).error())
        return rv;

    string module_name = name;
    string kernel_code = /*std+*/ code;

    array<const char*> headers_file_names;
    array<const char*> headers_texts;

    if(opt)
    {
        for(int i=0;i<opt->headers.size();i++)
        {
            headers_file_names.append(opt->headers.key(i)());
            headers_texts.append(opt->headers.value(i)());
        }
    }

    nvrtcProgram programm;
    if(nvrtcCreateProgram(&programm,kernel_code(),module_name(),headers_file_names.size(),headers_texts(),headers_file_names())!=NVRTC_SUCCESS)
    {
        setError(errorCodeCompilationFail);
        return rv;
    }

    string opt_arch  = "--gpu-architecture=compute_" + device_info[device].api_version;
    array<const char*> options;
    options.append(opt_arch());
    options.append("--relocatable-device-code=true");
    if(opt)
    {
        for(int i=0;i<opt->options.size();i++)
        {
            options.append(opt->options[i]());
        }
    }

    if(nvrtcCompileProgram(programm,options.size(),options()) != NVRTC_SUCCESS)
    {
        size_t log_size;
        if(nvrtcGetProgramLogSize(programm, &log_size) == NVRTC_SUCCESS)
        {
            string log_text(log_size,true);
            if(nvrtcGetProgramLog(programm, log_text()) == NVRTC_SUCCESS)
            {
                printLog(log_text);
            }
        }

        nvrtcDestroyProgram(&programm);
        setError(errorCodeCompilationFail);
        return rv;
    }

    // Obtain PTX from the program.
    size_t ptx_size = 0;
    if(nvrtcGetPTXSize(programm, &ptx_size)!=NVRTC_SUCCESS)
    {
        nvrtcDestroyProgram(&programm);
        setError(errorCodeCompilationFail);
        return rv;
    }
    byteArray ptx(ptx_size);
    if(nvrtcGetPTX(programm, (char*)ptx())!=NVRTC_SUCCESS)
    {
        nvrtcDestroyProgram(&programm);
        setError(errorCodeCompilationFail);
        return rv;
    }
    if(nvrtcDestroyProgram(&programm)!=NVRTC_SUCCESS)
    {
        setError(errorCodeCompilationFail);
        return rv;
    }

    CUmodule module;
    CUfunction kernel;

    if(opt && opt->libraries.size())
    {

        CUlinkState linkState;
        if(cuLinkCreate(0, 0, 0, &linkState) != CUDA_SUCCESS)
        {
            setError(errorCodeCompilationFail);
            return rv;
        }

        if(opt)
        {
            for(int i=0;i<opt->libraries.size();i++)
            {
                if(cuLinkAddData(linkState, CU_JIT_INPUT_LIBRARY, (void *)opt->libraries.value(i)(), opt->libraries.value(i).size(),
                                 opt->libraries.key(i)(), 0, 0, 0) != CUDA_SUCCESS)
                {
                    cuLinkDestroy(linkState);
                    setError(errorCodeCompilationFail);
                    return rv;
                }
            }
        }

        if(cuLinkAddData(linkState, CU_JIT_INPUT_PTX, (void *)ptx(), ptx_size, module_name(), 0, 0, 0) != CUDA_SUCCESS)
        {
            cuLinkDestroy(linkState);
            setError(errorCodeCompilationFail);
            return rv;
        }

        size_t cubinSize;
        void *cubin;
        if(cuLinkComplete(linkState, &cubin, &cubinSize) != CUDA_SUCCESS)
        {
            cuLinkDestroy(linkState);
            setError(errorCodeCompilationFail);
            return rv;
        }

        if(cuModuleLoadData(&module, cubin)!=CUDA_SUCCESS)
        {
            cuLinkDestroy(linkState);
            setError(errorCodeCompilationFail);
            return rv;
        }

        cuLinkDestroy(linkState);
    }
    else if(cuModuleLoadDataEx(&module, ptx(), 0, nullptr, nullptr) != CUDA_SUCCESS)
    {
        setError(errorCodeCompilationFail);
        return rv;
    }

    if(cuModuleGetFunction(&kernel, module, module_name()) != CUDA_SUCCESS)
    {
        cuModuleUnload(module);
        setError(errorCodeCompilationFail);
        return rv;
    }

    kernelStuff *ks = new kernelStuff;
    ks->name = module_name;
    ks->code = kernel_code;
    ks->module = module;
    ks->kernel = kernel;
    ks->arch = device_info[device].api_version;

    rv.kernel = ks;
    rv.device = device;
    rv.cluster = this;
    return rv;
}

retCode cudaCluster::kernelCall(const computeKernel &hand, dimensions<uint64> threads_block, dimensions<uint64> works_area)
{
    if(last_error.error()) return last_error;
    cudaStuff *stuff = reinterpret_cast<cudaStuff*>(internal_data);

    if(hand.cluster != this)
        return hand.cluster->kernelCall(hand, threads_block, works_area);

    attachDevice(hand.device);

    kernelStuff *ks = reinterpret_cast<kernelStuff*>(hand.kernel);

    dim3 grid;

    if(threads_block.size()>3 || works_area.size() > 3 || threads_block.size() != works_area.size() || !threads_block.size())
        return setError(errorCodeInvalidValue);

    grid.x = roundup_div(works_area[0],threads_block[0]);
    if(threads_block.size()>1)
        grid.y = roundup_div(works_area[1],threads_block[1]);
    if(threads_block.size()>2)
        grid.z = roundup_div(works_area[2],threads_block[2]);

    if(!grid.x || !grid.y || !grid.z || !threads_block.rawSize())
        return setError(errorCodeInvalidValue);

    uint64 smem_size = 0;
    array<void*> args;
    for(int i=0;i<hand.arguments.size();i++)
    {
        if(hand.arguments[i].cluster != this || hand.arguments[i].device != hand.device)
            return setError(errorCodeInvalidValue); //todo: auto transmit
        switch(hand.arguments[i].direction)
        {
        case computeArgument::LOCAL:
            smem_size += hand.arguments[i].item.toInt();
            break;
        default:
            args.append(hand.arguments[i].item.toPointer());
        }
    }

    cudaStream_t stream = hand.queue>1 ? stuff->streams[hand.device][hand.queue] : nullptr;

    if(smem_size > (48 << 10))
    {
        if(cuFuncSetAttribute(ks->kernel, CU_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES, smem_size)!=CUDA_SUCCESS)
            return setError(errorCodeInvalidValue);
    }

    if(cuLaunchKernel(ks->kernel,
                   grid.x, grid.y, grid.z,
                   threads_block[0], threads_block[1], threads_block[2],
                   smem_size,
                   stream,
                   args(),
                   nullptr) != CUDA_SUCCESS)
    {
        return setError(errorCodeLaunchFail);
    }

    if(hand.callback_user_data && !callback_kernel.empty())
    {
        callbackData *cbd = new callbackData;
        cbd->callback = callback_kernel;
        cbd->device = hand.device;
        cbd->cluster = this;
        cbd->queue = hand.queue;
        cbd->user_data = hand.callback_user_data;

        //cudaLaunchHostFunc(stream,&streamCallback,cbd); todo?
        return cudaErrorFilter(cudaStreamAddCallback(stream,&streamCallback,cbd,0));
    }

    return 0;
}

retCode cudaCluster::kernelConfig(computeKernel &hand, const string &symbol, const byteArray &data)
{
    if(last_error.error()) return last_error;
    cudaStuff *stuff = reinterpret_cast<cudaStuff*>(internal_data);

    if(hand.cluster != this)
        return hand.cluster->kernelConfig(hand, symbol, data);

    attachDevice(hand.device);

    kernelStuff *ks = reinterpret_cast<kernelStuff*>(hand.kernel);

    CUdeviceptr symbol_pointer = 0;
    size_t symbol_size = 0;
    if(cuModuleGetGlobal(&symbol_pointer, &symbol_size, ks->module, symbol()) != CUDA_SUCCESS)
        return setError(errorCodeInvalidValue);

    if(symbol_size != data.size())
        return setError(errorCodeInvalidValue);

    if(cuMemcpyHtoD(symbol_pointer, data(), data.size()) != CUDA_SUCCESS)
        return setError(errorCodeInvalidValue);

    return 0;
}

retCode cudaCluster::kernelDestroy(computeKernel &hand)
{
    if(last_error.error()) return last_error;
    cudaStuff *stuff = reinterpret_cast<cudaStuff*>(internal_data);

    if(hand.cluster != this)
        return hand.cluster->kernelDestroy(hand);

    attachDevice(hand.device);

    kernelStuff *ks = reinterpret_cast<kernelStuff*>(hand.kernel);

    if(cuModuleUnload(ks->module) != CUDA_SUCCESS)
    {
        return setError(errorCodeWrongDestruction);
    }

    delete ks;
    hand.kernel = nullptr;

    return 0;
}

computeMemory cudaCluster::memAlloc(int device, uint64 size, void *host_data, uint64 host_size, int queue, void *callback_user_data)
{
    computeMemory rv;
    if(last_error.error()) return rv;
    cudaStuff *stuff = reinterpret_cast<cudaStuff*>(internal_data);

    attachDevice(device);

    cudaStream_t stream = queue>1 ? stuff->streams[device][queue] : nullptr;

    void *mem;
    if(cudaErrorFilter(cudaMallocAsync((void **)&mem, size, stream)).error())
        return rv;

    if(host_data)
    {
        if(host_size == 1)
        {
            if(cudaErrorFilter(cudaMemsetAsync(mem, ((uint8*)host_data)[0], size, stream)).error())
            {
                cudaFreeAsync(mem,stream);
                return rv;
            }
        }
        else
        {
            for(size_t i = 0; i<roundup_div(size,host_size); i++)
            {
                uint64 curr_size = host_size;
                if((i+1)*host_size > size)
                    curr_size = size-i*host_size;
                if(cudaErrorFilter(cudaMemcpyAsync(((uint8*)mem)+i*host_size, host_data, curr_size, cudaMemcpyHostToDevice, stream)).error())
                {
                    cudaFreeAsync(mem,stream);
                    return rv;
                }
            }
        }
    }
    else if(callback_user_data)
    {
        setError(errorCodeInvalidValue);
    }

    rv.cluster = this;
    rv.device = device;
    rv.size = size;
    rv.item = variant(&mem,sizeof(mem));

    if(host_data && callback_user_data && !callback_memory.empty())
    {
        callbackData *cbd = new callbackData;
        cbd->callback = callback_kernel;
        cbd->device = device;
        cbd->cluster = this;
        cbd->queue = queue;
        cbd->user_data = callback_user_data;

        //cudaLaunchHostFunc(stream,&streamCallback,cbd); todo?
        cudaErrorFilter(cudaStreamAddCallback(stream,&streamCallback,cbd,0));
    }

    return rv;
}

retCode cudaCluster::memFree(computeMemory &hand, int queue)
{
    if(last_error.error()) return last_error;
    cudaStuff *stuff = reinterpret_cast<cudaStuff*>(internal_data);

    if(hand.cluster != this)
        return hand.cluster->memFree(hand);

    attachDevice(hand.device);

    cudaStream_t stream = queue>1 ? stuff->streams[hand.device][queue] : nullptr;

    if(cudaErrorFilter(cudaFreeAsync(*((void**)hand.item.toPointer()),stream)).error())
        return last_error;

    hand.item.clear();
    hand.size = 0;

    return 0;
}

retCode cudaCluster::memHostRead(void *host_dst, computeMemory &src, uint64 size, uint64 src_offset, int queue, void *callback_user_data)
{
    if(last_error.error()) return last_error;
    cudaStuff *stuff = reinterpret_cast<cudaStuff*>(internal_data);

    if(src.cluster != this)
        return src.cluster->memHostRead(host_dst, src, size, src_offset, queue, callback_user_data);

    attachDevice(src.device);

    cudaStream_t stream = queue>1 ? stuff->streams[src.device][queue] : nullptr;

    if(cudaErrorFilter(cudaMemcpyAsync(host_dst, (*((uint8**)src.item.toPointer()))+src_offset, size, cudaMemcpyDeviceToHost, stream)).error())
        return last_error;

    if(callback_user_data && !callback_memory.empty())
    {
        callbackData *cbd = new callbackData;
        cbd->callback = callback_kernel;
        cbd->device = src.device;
        cbd->cluster = this;
        cbd->queue = queue;
        cbd->user_data = callback_user_data;

        //cudaLaunchHostFunc(stream,&streamCallback,cbd); todo?
        return cudaErrorFilter(cudaStreamAddCallback(stream,&streamCallback,cbd,0));
    }

    return 0;
}

retCode cudaCluster::memHostWrite(computeMemory &dst, void *host_src, uint64 size, uint64 dst_offset, uint64 host_size, int queue, void *callback_user_data)
{
    if(last_error.error()) return last_error;
    cudaStuff *stuff = reinterpret_cast<cudaStuff*>(internal_data);

    if(dst.cluster != this)
        return dst.cluster->memHostWrite(dst, host_src, size, dst_offset, host_size, queue, callback_user_data);

    attachDevice(dst.device);

    cudaStream_t stream = queue>1 ? stuff->streams[dst.device][queue] : nullptr;

    if(host_size == 1)
    {
        if(cudaErrorFilter(cudaMemsetAsync((*((uint8**)dst.item.toPointer()))+dst_offset, ((uint8*)host_src)[0], size, stream)).error())
            return last_error;
    }
    else
    {
        for(size_t i = 0; i<roundup_div(size,host_size); i++)
        {
            uint64 curr_size = host_size;
            if((i+1)*host_size > size)
                curr_size = size-i*host_size;
            if(cudaErrorFilter(cudaMemcpyAsync( (*((uint8**)dst.item.toPointer()))+dst_offset + i*host_size,
                                                host_src, curr_size, cudaMemcpyDeviceToHost, stream)).error())
                return last_error;
        }
    }

    if(callback_user_data && !callback_memory.empty())
    {
        callbackData *cbd = new callbackData;
        cbd->callback = callback_kernel;
        cbd->device = dst.device;
        cbd->cluster = this;
        cbd->queue = queue;
        cbd->user_data = callback_user_data;

        //cudaLaunchHostFunc(stream,&streamCallback,cbd); todo?
        return cudaErrorFilter(cudaStreamAddCallback(stream,&streamCallback,cbd,0));
    }

    return 0;
}

retCode cudaCluster::memCopy(computeMemory &dst, computeMemory &src, uint64 size, uint64 dst_offset, uint64 src_offset, int dst_queue, int src_queue, void *callback_user_data)
{
    if(last_error.error()) return last_error;
    cudaStuff *stuff = reinterpret_cast<cudaStuff*>(internal_data);

    if(dst.cluster!=this || src.cluster!=this || dst.device!=src.device)
        computeCluster::memCopy(dst, src, size, dst_offset, src_offset, dst_queue, src_queue, callback_user_data);

    if(dst_queue!=src_queue)
    {
        //todo: more smart solution
        if(cudaErrorFilter(sync(dst.device,src_queue)).error())
            return last_error;
    }

    cudaStream_t stream = dst_queue>1 ? stuff->streams[dst.device][dst_queue] : nullptr;

    if(cudaErrorFilter(cudaMemcpyAsync((*(uint8**)dst.item.toPointer())+dst_offset, (*(uint8**)src.item.toPointer())+src_offset, size, cudaMemcpyDeviceToDevice, stream)).error())
        return last_error;

    if(callback_user_data && !callback_memory.empty())
    {
        callbackData *cbd = new callbackData;
        cbd->callback = callback_kernel;
        cbd->device = dst.device;
        cbd->cluster = this;
        cbd->queue = dst_queue;
        cbd->user_data = callback_user_data;

        //cudaLaunchHostFunc(stream,&streamCallback,cbd); todo?
        return cudaErrorFilter(cudaStreamAddCallback(stream,&streamCallback,cbd,0));
    }

    return 0;
}
