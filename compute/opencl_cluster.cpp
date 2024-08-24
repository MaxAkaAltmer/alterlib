#include "opencl_cluster.h"

#include <CL/cl.h>

using namespace alt;
using namespace compute;

static retCode openclTranslateError(int err)
{
    //todo: finish and make nore ordered

    if(CL_SUCCESS == err)
        return 0;

    return -1;
}

retCode openclCluster::openclErrorFilter(int err)
{
    if(err != CL_SUCCESS)
        printLog("OpenCL error: " + string::fromInt(err)+"\n");

    return setError(openclTranslateError(err));
}

struct openclDeviceItem
{
    int platform_index = -1;
    cl_platform_id platform = nullptr;
    cl_device_id device = nullptr;
};

struct openclStuff
{
    hash<int,array<cl_command_queue>>   queues;
    array<openclDeviceItem> devices;
    array<cl_context> contexts;
};

struct kernelStuff
{
    string name;
    string code;

    cl_program        program = nullptr;
    cl_kernel         kernel = nullptr;
};

static deviceInfo makeOpenclDeviceInfo(cl_platform_id platform, cl_device_id device, int index)
{
    deviceInfo inf;

    size_t size = 0;
    if((clGetDeviceInfo(device,CL_DEVICE_NAME,0,nullptr,&size)) != CL_SUCCESS || !size)
        return deviceInfo();
    inf.name.resize(size-1);
    if(clGetDeviceInfo(device,CL_DEVICE_NAME,size,inf.name(),nullptr))
        return deviceInfo();

    if(clGetPlatformInfo(platform,CL_PLATFORM_NAME,0,nullptr,&size) != CL_SUCCESS || !size)
        return deviceInfo();
    string platform_name;
    platform_name.resize(size-1);
    if(clGetPlatformInfo(platform,CL_PLATFORM_NAME,size,platform_name(),nullptr) != CL_SUCCESS)
        return deviceInfo();

    inf.name = platform_name + " -> " + inf.name;

    inf.index = index;
    inf.api_type = "opencl";

    if(clGetDeviceInfo(device,CL_DEVICE_VERSION,0,nullptr,&size) != CL_SUCCESS || !size)
        return deviceInfo();
    inf.api_version.resize(size-1);
    if(clGetDeviceInfo(device,CL_DEVICE_VERSION,size,inf.api_version(),nullptr) != CL_SUCCESS)
        return deviceInfo();

    cl_uint dims = 0;
    if(clGetDeviceInfo(device,CL_DEVICE_MAX_WORK_ITEM_DIMENSIONS,sizeof(cl_uint),&dims,nullptr) != CL_SUCCESS)
        return deviceInfo();

    array<size_t> dims_temp(dims);
    inf.max_threads_per_dimension.resize(dims);
    inf.max_grid_size.resize(dims);
    if(clGetDeviceInfo(device,CL_DEVICE_MAX_WORK_ITEM_SIZES,sizeof(size_t)*dims,dims_temp(),nullptr) != CL_SUCCESS)
        return deviceInfo();
    for(cl_uint i=0;i<dims;i++)
    {
        inf.max_threads_per_dimension[i] = dims_temp[i];
        inf.max_grid_size[i] = CL_INT_MAX; //will working!?
    }

    size_t size_t_value = 0;
    if(clGetDeviceInfo(device,CL_DEVICE_MAX_WORK_GROUP_SIZE,sizeof(size_t),&size_t_value,nullptr) != CL_SUCCESS)
        return deviceInfo();
    inf.max_threads_total = size_t_value;

    cl_ulong ulong_value = 0;
    if(clGetDeviceInfo(device,CL_DEVICE_GLOBAL_MEM_SIZE,sizeof(cl_ulong),&ulong_value,nullptr) != CL_SUCCESS)
        return deviceInfo();
    inf.global_memory_size = ulong_value;

    ulong_value = 0;
    if(clGetDeviceInfo(device,CL_DEVICE_LOCAL_MEM_SIZE,sizeof(cl_ulong),&ulong_value,nullptr) != CL_SUCCESS)
        return deviceInfo();
    inf.shared_memory_size = ulong_value;

    ulong_value = 0;
    if(clGetDeviceInfo(device,CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE,sizeof(cl_ulong),&ulong_value,nullptr) != CL_SUCCESS)
        return deviceInfo();
    inf.constant_memory_size = ulong_value;

    size_t_value = 0;
    if(clGetDeviceInfo(device,CL_DEVICE_PREFERRED_WORK_GROUP_SIZE_MULTIPLE,sizeof(size_t),&size_t_value,nullptr) == CL_SUCCESS)
            inf.max_warp_size = size_t_value;

    cl_uint uint_value = 0;
    if(clGetDeviceInfo(device,CL_DEVICE_MAX_COMPUTE_UNITS,sizeof(cl_uint),&uint_value,nullptr) != CL_SUCCESS)
        return deviceInfo();
    inf.total_cores = uint_value;

    return inf;
}


openclCluster::openclCluster()
    :computeCluster()
{
    cl_uint count = 0;
    if(openclErrorFilter(clGetPlatformIDs( 0, nullptr, &count )).error())
        return;

    array<cl_platform_id> platforms(count);
    if(openclErrorFilter(clGetPlatformIDs( count, platforms(), nullptr )).error())
        return;

    internal_data = new openclStuff;
    openclStuff *stuff = reinterpret_cast<openclStuff*>(internal_data);

    array<cl_device_id> dev_list;

    cl_int ret_code = 0;

    for(cl_uint i=0;i<platforms.size();i++)
    {
        cl_uint dev_count = 0;
        if(openclErrorFilter(clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, 0, nullptr, &dev_count)).error())
            return;

        if(!dev_count) continue;
        array<cl_device_id> dev(dev_count);

        if(openclErrorFilter(clGetDeviceIDs(platforms[i], CL_DEVICE_TYPE_ALL, dev_count, dev(), nullptr)).error())
            return;

        for(cl_uint d = 0; d<dev_count;d++)
        {
            openclDeviceItem item;
            item.device = dev[d];
            item.platform = platforms[i];
            item.platform_index = i;

            deviceInfo inf = makeOpenclDeviceInfo(platforms[i], dev[d], stuff->devices.size());
            if(inf.index>=0)
            {
                stuff->devices.append(item);
                device_info.append(inf);
                dev_list.append(item.device);
            }
        }

        stuff->contexts.append(clCreateContext(nullptr, dev_list.size(), dev_list(), nullptr, nullptr, &ret_code));
        if(openclErrorFilter(ret_code).error())
            return;

        dev_list.clear();

    }

    for(intz i = 0; i<device_info.size(); i++)
    {
        cl_command_queue queue = clCreateCommandQueueWithProperties(stuff->contexts[stuff->devices[i].platform_index],
                                                                    stuff->devices[i].device, nullptr, &ret_code);
        if(openclErrorFilter(ret_code).error())
            return;

        stuff->queues[i].append(queue);
    }
}

openclCluster::~openclCluster()
{
    if(!internal_data)
        return;

    openclStuff *stuff = reinterpret_cast<openclStuff*>(internal_data);

    for(int i=0;i<stuff->queues.size();i++)
    {
        for(intz j=0;j<stuff->queues.value(i).size();j++)
            clReleaseCommandQueue(stuff->queues.value(i)[j]);
    }
    for(int i=0;i<stuff->contexts.size();i++)
        clReleaseContext(stuff->contexts[i]);

    delete stuff;
}

retCode openclCluster::sync(int device, int queue)
{
    if(last_error.error()) return last_error;
    openclStuff *stuff = reinterpret_cast<openclStuff*>(internal_data);

    if(queue<0)
    {
        for(intz i = 0; stuff->queues[device].size(); i++)
        {
            if(openclErrorFilter(clFlush(stuff->queues[device][i])).error())
                return last_error;
            if(openclErrorFilter(clFinish(stuff->queues[device][i])).error())
                return last_error;
        }
    }
    else
    {
        if(stuff->queues[device].size()<=queue)
        {
            last_error = errorCodeInvalidValue;
            return last_error;
        }
        if(openclErrorFilter(clFlush(stuff->queues[device][queue])).error())
            return last_error;
        if(openclErrorFilter(clFinish(stuff->queues[device][queue])).error())
            return last_error;
    }
    return 0;
}

retCode openclCluster::initExtraQueues(int device, int count)
{
    if(last_error.error()) return last_error;
    openclStuff *stuff = reinterpret_cast<openclStuff*>(internal_data);

    int curr_size = stuff->queues[device].size();
    count++;

    if(curr_size == count)
        return 0;

    cl_int ret_code = 0;

    if(curr_size < count)
    {
        stuff->queues[device].resize(count);

        for(int i=curr_size;i<count;i++)
        {
            stuff->queues[device][i] = clCreateCommandQueueWithProperties(stuff->contexts[stuff->devices[device].platform_index],
                                                                            stuff->devices[device].device, nullptr, &ret_code);
            if(openclErrorFilter(ret_code).error())
                return last_error;
        }
    }

    if(curr_size > count)
    {
        for(int i=count;i<curr_size;i++)
        {
            clReleaseCommandQueue(stuff->queues[device][i]);
        }
        stuff->queues[device].resize(count);
    }

    return 0;
}

computeKernel openclCluster::kernelCompile(int device, const string &name, const string &code, const compileOptions *opt)
{
    computeKernel rv;
    if(last_error.error()) return rv;
    openclStuff *stuff = reinterpret_cast<openclStuff*>(internal_data);

    string module_name = name;
    string kernel_code = /*std+*/ code;

    array<const char*> headers_file_names;
    array<const char*> headers_texts;

    cl_int ret_code = 0;

    string compiler_options;

    if(opt)
    {
        for(int i=0;i<opt->headers.size();i++)
        {
            headers_file_names.append(opt->headers.key(i)());
            headers_texts.append(opt->headers.value(i)());
        }
        compiler_options = string::join(opt->options," ");
    }

    array<const char*> code_ptrs;

    //todo: C preprocessing
    for(int i=0;i<headers_texts.size();i++)
    {
        code_ptrs.append(headers_texts[i]);
    }

    code_ptrs.append(kernel_code());

    cl_program programm = clCreateProgramWithSource(stuff->contexts[stuff->devices[device].platform_index],
                                                    code_ptrs.size(), code_ptrs(), nullptr, &ret_code);
    if(openclErrorFilter(ret_code).error())
        return rv;

    ret_code = clBuildProgram(programm, 1, &stuff->devices[device].device, compiler_options(), nullptr, nullptr);
    if(openclErrorFilter(ret_code).error())
    {
        size_t len = 0;
        ret_code = clGetProgramBuildInfo(programm, stuff->devices[device].device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &len);
        if(openclErrorFilter(ret_code).error())
            return rv;

        if(len)
        {
            string buffer;
            buffer.resize(len);
            ret_code = clGetProgramBuildInfo(programm, stuff->devices[device].device, CL_PROGRAM_BUILD_LOG, len, buffer(), nullptr);
            printLog(buffer.trimmed());
        }

        return rv;
    }

    cl_kernel kernel = clCreateKernel(programm, module_name(), &ret_code);
    if(openclErrorFilter(ret_code).error())
        return rv;

    kernelStuff *ks = new kernelStuff;
    ks->name = module_name;
    ks->code = kernel_code;
    ks->program = programm;
    ks->kernel = kernel;

    rv.kernel = ks;
    rv.device = device;
    rv.cluster = this;
    return rv;

    return rv;
}

struct callbackData
{
    void *user_data;
    openclCluster *cluster;
    int device;
    int queue;

    delegateProc<retCode,void*/*callback_user_data*/,computeCluster*,int/*device*/,int/*queue*/> callback;
};

static void CL_CALLBACK  opencl_event_notify(cl_event event, cl_int event_command_exec_status, void *user_data)
{
    callbackData *d = reinterpret_cast<callbackData*>(user_data);
    d->callback(openclTranslateError(clReleaseEvent(event)),d->user_data,d->cluster,d->device,d->queue);

    delete d;
}

retCode openclCluster::kernelCall(const computeKernel &hand, dimensions<uint64> threads_block, dimensions<uint64> works_area)
{
    if(last_error.error()) return last_error;
    openclStuff *stuff = reinterpret_cast<openclStuff*>(internal_data);

    if(hand.cluster != this)
        return hand.cluster->kernelCall(hand, threads_block, works_area);

    kernelStuff *ks = reinterpret_cast<kernelStuff*>(hand.kernel);

    if(threads_block.size()>3 || works_area.size() > 3 || threads_block.size() != works_area.size() || !threads_block.size())
        return setError(errorCodeInvalidValue);

    if(!works_area.rawSize() || !threads_block.rawSize())
        return setError(errorCodeInvalidValue);

    dimensions<size_t> block = threads_block;
    dimensions<size_t> works = works_area;
    works.roundup(block);

    uint64 smem_size = 0;
    array<void*> args;
    for(int i=0;i<hand.arguments.size();i++)
    {
        if(hand.arguments[i].cluster != this || hand.arguments[i].device != hand.device)
            return setError(errorCodeInvalidValue); //todo: auto transmit

        if(hand.arguments[i].item.isData())
        {
            if(openclErrorFilter(clSetKernelArg(ks->kernel, i, hand.arguments[i].item.size(), hand.arguments[i].item.toPointer())).error())
                return last_error;
        }
        else if(hand.arguments[i].item.isInt())
        {
            if(openclErrorFilter(clSetKernelArg(ks->kernel, i, hand.arguments[i].item.size(), nullptr)).error())
                return last_error;
        }
        else
        {
            return setError(errorCodeInvalidValue);
        }
    }

    cl_event event = nullptr;
    bool create_event = hand.callback_user_data && !callback_kernel.empty();

    if(openclErrorFilter(clEnqueueNDRangeKernel(stuff->queues[hand.device][hand.queue], ks->kernel,
                                                block.size(), nullptr, works(), block(),
                                                0, nullptr, create_event?&event:nullptr)).error())
        return last_error;

    if(create_event && !event)
        return setError(errorCodeLaunchFail);

    if(event)
    {
        callbackData *cbd = new callbackData;
        cbd->callback = callback_kernel;
        cbd->device = hand.device;
        cbd->cluster = this;
        cbd->queue = hand.queue;
        cbd->user_data = hand.callback_user_data;

        return openclErrorFilter(clSetEventCallback ( event, CL_COMPLETE , &opencl_event_notify, cbd));
    }

    return 0;
}
retCode openclCluster::kernelConfig(computeKernel &hand, const string &symbol, const byteArray &data)
{
    if(last_error.error()) return last_error;
    openclStuff *stuff = reinterpret_cast<openclStuff*>(internal_data);

    if(hand.cluster != this)
        return hand.cluster->kernelConfig(hand, symbol, data);

    kernelStuff *ks = reinterpret_cast<kernelStuff*>(hand.kernel);

    //unsupported in opencl?
    //todo?: over rebuild
    return setError(errorCodeInvalidSymbol);
}
retCode openclCluster::kernelDestroy(computeKernel &hand)
{
    if(last_error.error()) return last_error;
    openclStuff *stuff = reinterpret_cast<openclStuff*>(internal_data);

    if(hand.cluster != this)
        return hand.cluster->kernelDestroy(hand);

    kernelStuff *ks = reinterpret_cast<kernelStuff*>(hand.kernel);

    if(clReleaseKernel(ks->kernel) != CL_SUCCESS)
        return setError(errorCodeWrongDestruction);
    if(clReleaseProgram(ks->program) != CL_SUCCESS)
        return setError(errorCodeWrongDestruction);

    delete ks;
    hand.kernel = nullptr;

    return 0;
}

computeMemory openclCluster::memAlloc(int device, uint64 size, void *host_data, uint64 host_size, int queue, void *callback_user_data)
{
    computeMemory rv;
    if(last_error.error()) return rv;
    openclStuff *stuff = reinterpret_cast<openclStuff*>(internal_data);

    cl_mem_flags flags = CL_MEM_READ_WRITE;

    cl_int ret_code = 0;
    cl_mem mem = nullptr;

    cl_event event = nullptr;

    if(!host_data)
    {
        mem = clCreateBuffer(stuff->contexts[stuff->devices[device].platform_index], flags, size, host_data, &ret_code);
        if(callback_user_data)
            setError(errorCodeInvalidValue);
    }
    else
    {
        bool create_event = callback_user_data && !callback_memory.empty();

        mem = clCreateBuffer(stuff->contexts[stuff->devices[device].platform_index], flags, size, nullptr, &ret_code);

        if(openclErrorFilter(ret_code).error())
            return rv;

        if(host_size == size)
        {
            openclErrorFilter(clEnqueueWriteBuffer(stuff->queues[device][queue],
                mem, CL_FALSE, 0, size, host_data,
                0, /*num_events_in_wait_list*/
                nullptr, /*event_wait_list*/
                create_event?&event:nullptr));
        }
        else
        {
            openclErrorFilter(clEnqueueFillBuffer(stuff->queues[device][queue],
                mem, host_data, host_size, 0, size,
                0 /*num_events_in_wait_list*/,
                nullptr /*event_wait_list*/,
                create_event?&event:nullptr));
        }

        if(callback_user_data && !event)
            setError(errorCodeLaunchFail);

    }

    openclErrorFilter(ret_code);

    rv.cluster = this;
    rv.device = device;
    rv.size = size;
    rv.item = variant(&mem,sizeof(mem));

    if(event)
    {
        callbackData *cbd = new callbackData;
        cbd->callback = callback_memory;
        cbd->device = device;
        cbd->cluster = this;
        cbd->queue = queue;
        cbd->user_data = callback_user_data;

        openclErrorFilter(clSetEventCallback ( event, CL_COMPLETE , &opencl_event_notify, cbd));
    }

    return rv;
}
retCode openclCluster::memFree(computeMemory &hand, int queue)
{
    if(last_error.error()) return last_error;
    openclStuff *stuff = reinterpret_cast<openclStuff*>(internal_data);

    if(hand.cluster != this)
        return hand.cluster->memFree(hand);

    cl_mem ptr = hand.item.deserialize<cl_mem>();
    if(openclErrorFilter(clReleaseMemObject(ptr)).error())
        return last_error;

    hand.item.clear();
    hand.size = 0;

    return 0;
}
retCode openclCluster::memHostRead(void *host_dst, computeMemory &src, uint64 size, uint64 src_offset, int queue, void *callback_user_data)
{
    if(last_error.error()) return last_error;
    openclStuff *stuff = reinterpret_cast<openclStuff*>(internal_data);

    if(src.cluster != this)
        return src.cluster->memHostRead(host_dst, src, size, src_offset, queue, callback_user_data);

    cl_event event = nullptr;
    bool create_event = callback_user_data && !callback_memory.empty();
    cl_mem ptr = src.item.deserialize<cl_mem>();
    if(openclErrorFilter(clEnqueueReadBuffer(
            stuff->queues[src.device][queue],
            ptr,
            CL_FALSE,
            src_offset,
            size,
            host_dst,
            0, /*num_events_in_wait_list*/
            nullptr, /*event_wait_list*/
            create_event?&event:nullptr)).error())
        return last_error;

    if(callback_user_data && !event)
        setError(errorCodeLaunchFail);

    if(event)
    {
        callbackData *cbd = new callbackData;
        cbd->callback = callback_kernel;
        cbd->device = src.device;
        cbd->cluster = this;
        cbd->queue = queue;
        cbd->user_data = callback_user_data;

        return openclErrorFilter(clSetEventCallback ( event, CL_COMPLETE , &opencl_event_notify, cbd));
    }

    return 0;
}
retCode openclCluster::memHostWrite(computeMemory &dst, void *host_src, uint64 size, uint64 dst_offset, uint64 host_size, int queue, void *callback_user_data)
{
    if(last_error.error()) return last_error;
    openclStuff *stuff = reinterpret_cast<openclStuff*>(internal_data);

    if(dst.cluster != this)
        return dst.cluster->memHostWrite(dst, host_src, size, dst_offset, host_size, queue, callback_user_data);

    cl_event event = nullptr;
    bool create_event = callback_user_data && !callback_memory.empty();
    cl_mem ptr = dst.item.deserialize<cl_mem>();

    if(host_size == size)
    {
        if(openclErrorFilter(clEnqueueWriteBuffer(stuff->queues[dst.device][queue],
                                ptr, CL_FALSE, dst_offset, size, host_src,
                                0, /*num_events_in_wait_list*/
                                nullptr, /*event_wait_list*/
                                create_event?&event:nullptr)).error())
            return last_error;
    }
    else
    {
        if(openclErrorFilter(clEnqueueFillBuffer(stuff->queues[dst.device][queue],
                                ptr, host_src, host_size, dst_offset, size,
                                0 /*num_events_in_wait_list*/,
                                nullptr /*event_wait_list*/,
                                create_event?&event:nullptr)).error())
            return last_error;
    }

    if(callback_user_data && !event)
        setError(errorCodeLaunchFail);

    if(event)
    {
        callbackData *cbd = new callbackData;
        cbd->callback = callback_kernel;
        cbd->device = dst.device;
        cbd->cluster = this;
        cbd->queue = queue;
        cbd->user_data = callback_user_data;

        return openclErrorFilter(clSetEventCallback ( event, CL_COMPLETE , &opencl_event_notify, cbd));
    }

    return 0;
}
retCode openclCluster::memCopy(computeMemory &dst, computeMemory &src, uint64 size,
                uint64 dst_offset, uint64 src_offset,
                int dst_queue, int src_queue,
                void *callback_user_data)
{
    if(last_error.error()) return last_error;
    openclStuff *stuff = reinterpret_cast<openclStuff*>(internal_data);

    if(dst.cluster!=this || src.cluster!=this || dst.device!=src.device)
        computeCluster::memCopy(dst, src, size, dst_offset, src_offset, dst_queue, src_queue, callback_user_data);

    if(dst_queue!=src_queue)
    {
        //todo: more smart solution
        if(openclErrorFilter(sync(dst.device,src_queue)).error())
            return last_error;
    }

    cl_mem src_ptr = src.item.deserialize<cl_mem>();
    cl_mem dst_ptr = dst.item.deserialize<cl_mem>();

    cl_event event = nullptr;
    bool create_event = callback_user_data && !callback_memory.empty();

    if(openclErrorFilter(clEnqueueCopyBuffer(stuff->queues[dst.device][dst_queue],
                        src_ptr,
                        dst_ptr,
                        src_offset,
                        dst_offset,
                        size,
                        0,
                        nullptr,
                        create_event?&event:nullptr)).error())
        return last_error;

    if(event)
    {
        callbackData *cbd = new callbackData;
        cbd->callback = callback_kernel;
        cbd->device = dst.device;
        cbd->cluster = this;
        cbd->queue = dst_queue;
        cbd->user_data = callback_user_data;

        return openclErrorFilter(clSetEventCallback ( event, CL_COMPLETE , &opencl_event_notify, cbd));
    }

    return 0;
}
