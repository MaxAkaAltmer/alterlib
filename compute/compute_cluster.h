#ifndef COMPUTEDEVICE_H
#define COMPUTEDEVICE_H

#include "../atypes.h"
#include "../astring.h"
#include "../at_hash.h"
#include "../at_tensor.h"
#include "../adelegate.h"
#include <typeinfo>

namespace alt::compute {

class computeCluster;
struct computeKernel;

struct computeMemory
{
    computeCluster* cluster = nullptr;
    int device = -1;
    variant item;
    uint64 size = 0;
};


struct deviceInfo
{
    string name;
    int index;

    string api_type;
    string api_version;

    dimensions<uint64> max_threads_per_dimension;
    uint64 max_threads_total;
    dimensions<uint64> max_grid_size;
    uint64 max_warp_size = 0;
    uint64 total_cores = 1;

    uint64 global_memory_size;
    uint64 shared_memory_size = 0;
    uint64 constant_memory_size = 0;

    hash<string,variant> api_related;

    string toText()
    {
        string rv = name + " [" + string::fromInt(index) + "] "+api_type+" "+api_version
                +" (BLOCK:"+max_threads_per_dimension.toString()+"<="+string::fromInt(max_threads_total)
                +",GRID:"+max_grid_size.toString() + ",WARP:"+string::fromInt(max_warp_size) + ",CORE:" + string::fromInt(total_cores)
                +",RAM:"+string::fromInt(global_memory_size)+",SMEM:"+string::fromInt(shared_memory_size)
                +",CMEM:"+string::fromInt(constant_memory_size)+")";
        for(int i=0;i<api_related.size();i++)
            rv += "\n\t"+api_related.key(i)+" = "+api_related.value(i).toString();
        return rv+"\n";
    }
};

enum computeErrorCodes
{
    //todo: make more detailed errors
    errorCodeInitFail = -2,
    errorCodeInvalidValue = -3,
    errorCodeMemoryAllocation = -4,
    errorCodeProfilerDisabled = -5,
    errorCodeWrongDestruction = -6,
    errorCoderOutOfResources = -7,
    errorCodeInvalidSymbol = -8,
    errorCodeDeviceUnavailable = -9
};

class computeCluster
{
public:
    computeCluster();
    virtual ~computeCluster() {}

    virtual retCode sync(int device, int queue = -1) { return 0; };

    const array<deviceInfo>& deviceList() { return device_info; }
    virtual retCode initExtraQueues(int device, int count) = 0;

    void regKernelCallback(delegateProc<retCode,void*/*callback_user_data*/,computeCluster*,int/*device*/,int/*queue*/> callback)
    {
        callback_kernel = callback;
    }
    void regMemoryCallback(delegateProc<retCode,void*/*callback_user_data*/,computeCluster*,int/*device*/,int/*queue*/> callback)
    {
        callback_memory = callback;
    }

    virtual computeKernel kernelCompile(int device, const string &name, const string &code, const string &options = string()) = 0;
    virtual retCode kernelCall(const computeKernel &hand, dimensions<uint64> threads_block, dimensions<uint64> works_area) = 0;
    virtual retCode kernelConfig(computeKernel &hand, const string &symbol, const byteArray &data) = 0;
    virtual retCode kernelDestroy(computeKernel &hand) = 0;

    virtual computeMemory memAlloc(int device, uint64 size, void *host_data = nullptr, uint64 host_size = 0, int queue = 0, void *callback_user_data = nullptr) = 0;
    virtual retCode memFree(computeMemory &hand) = 0;

    virtual retCode memHostRead(void *host_dst, computeMemory &src, uint64 size, uint64 src_offset = 0, int queue = 0, void *callback_user_data = nullptr) = 0;
    virtual retCode memHostWrite(computeMemory &dst, void *host_src, uint64 size, uint64 dst_offset = 0, uint64 host_size = 0, int queue = 0, void *callback_user_data = nullptr) = 0;

    virtual retCode memCopy(computeMemory &dst, computeMemory &src, uint64 size,
                            uint64 dst_offset = 0, uint64 src_offset = 0,
                            int dst_queue = 0, int src_queue = 0,
                            void *callback_user_data = nullptr)
    {
        if(last_error.error())
            return last_error;

        array<uint8> temp(size);

        if(setError(src.cluster->memHostRead(temp(),src,size,src_offset,src_queue)).error())
            return last_error;
        if(setError(src.cluster->sync(src.device,src_queue)).error())
            return last_error;

        if(setError(dst.cluster->memHostWrite(dst,temp(),size,dst_offset,0,dst_queue,callback_user_data)).error())
            return last_error;
        return setError(src.cluster->sync(dst.device,dst_queue));
    }

    virtual retCode processError()
    {
        retCode rv = last_error;
        last_error.clear();
        return last_error;
    }

    static string retCodeToString(retCode code);

protected:

    delegateProc<retCode,void*/*callback_user_data*/,computeCluster*,int/*device*/,int/*queue*/> callback_memory, callback_kernel;

    retCode setError(retCode code)
    {
        if(code.error())
            last_error = code;
        return code;
    }

    void printLog(string text);

    retCode last_error;

    array<deviceInfo> device_info;

};

struct computeArgument: public computeMemory
{
    enum Dir
    {
        IN = 1,
        OUT = -1,
        INOUT = 0,
        LOCAL = -2,
        VALUE = 2
    };

    int direction = VALUE;
};

__inline computeArgument in(const computeMemory &mem)
{
    computeArgument rv;
    rv.cluster = mem.cluster;
    rv.device = mem.device;
    rv.item = mem.item;
    rv.size = mem.size;
    rv.direction = computeArgument::IN;
    return rv;
}

__inline computeArgument out(const computeMemory &mem)
{
    computeArgument rv;
    rv.cluster = mem.cluster;
    rv.device = mem.device;
    rv.item = mem.item;
    rv.size = mem.size;
    rv.direction = computeArgument::OUT;
    return rv;
}

__inline computeArgument inout(const computeMemory &mem)
{
    computeArgument rv;
    rv.cluster = mem.cluster;
    rv.device = mem.device;
    rv.item = mem.item;
    rv.size = mem.size;
    rv.direction = computeArgument::INOUT;
    return rv;
}

__inline computeArgument local(uint32 size)
{
    computeArgument rv;
    rv.item = variant(size);
    rv.direction = computeArgument::LOCAL;
    return rv;
}

struct computeKernel
{
    computeCluster* cluster = nullptr;
    int device = -1;
    void *kernel = nullptr;

    int queue = 0;
    void *callback_user_data = nullptr;

    array<computeArgument> arguments;

    void setArg(int off, uint64 size, void *ptr)
    {
        if(arguments.size()<off+1)
            arguments.resize(off+1);
        arguments[off].cluster = nullptr;
        arguments[off].device = -1;
        arguments[off].size = 0;
        if(ptr)
        {
            arguments[off].direction = computeArgument::VALUE;
            arguments[off].item = variant(ptr,size);
        }
        else
        {
            arguments[off].direction = computeArgument::LOCAL;
            arguments[off].item = variant(size);
        }
    }

    void setArg(int off, computeMemory mem)
    {
        if(arguments.size()<off+1)
            arguments.resize(off+1);
        arguments[off].cluster = mem.cluster;
        arguments[off].device = mem.device;
        arguments[off].item = mem.item;
        arguments[off].size = mem.size;
        arguments[off].direction = computeArgument::INOUT;
    }

    void setArg(int off, computeArgument arg)
    {
        if(arguments.size()<off+1)
            arguments.resize(off+1);
        arguments[off] = arg;
    }

    template <typename... T>
    void setArgs(int off, T ...args)
    {
        int i=0;
        ([&]{
            if(typeid(args)==typeid(computeMemory) || typeid(args)==typeid(computeMemory&)
                    || typeid(args)==typeid(computeArgument) || typeid(args)==typeid(computeArgument&))
                setArg(i+off,args);
            else
                setArg(i+off,sizeof(args), (void *)&args);
            i++;
        }(), ...);
    }

    template <typename... T>
    retCode operator()(dimensions<uint64> threads_block, dimensions<uint64> works_area, T ...args)
    {
        int i=0;
        ([&]{
            if(typeid(args)==typeid(computeMemory) || typeid(args)==typeid(computeMemory&)
                    || typeid(args)==typeid(computeArgument) || typeid(args)==typeid(computeArgument&))
                setArg(i,args);
            else
                setArg(i,sizeof(args), (void *)&args);
            i++;
        }(), ...);

        return cluster->kernelCall(*this,threads_block,works_area);
    }
};

} //alt::compute namespace

#endif // COMPUTEDEVICE_H
