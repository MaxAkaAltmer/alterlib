#ifndef CUDADEVICE_H
#define CUDADEVICE_H

#include "compute_cluster.h"

namespace alt::compute {

class cudaCluster: public computeCluster
{
public:
    cudaCluster();
    ~cudaCluster() override;

    retCode sync(int device, int queue = -1) override;
    retCode initExtraQueues(int device, int count) override;

    computeKernel kernelCompile(int device, const string &name, const string &code, const string &options = string()) override;
    retCode kernelCall(const computeKernel &hand, dimensions<uint64> threads_block, dimensions<uint64> works_area) override;
    retCode kernelConfig(computeKernel &hand, const string &symbol, const byteArray &data) override;
    retCode kernelDestroy(computeKernel &hand) override;

    computeMemory memAlloc(int device, uint64 size, void *host_data = nullptr, uint64 host_size = 0, int queue = 0, void *callback_user_data = nullptr) override;
    retCode memFree(computeMemory &hand) override;
    retCode memHostRead(void *host_dst, computeMemory &src, uint64 size, uint64 src_offset = 0, int queue = 0, void *callback_user_data = nullptr) override;
    retCode memHostWrite(computeMemory &dst, void *host_src, uint64 size, uint64 dst_offset = 0, uint64 host_size = 0, int queue = 0, void *callback_user_data = nullptr) override;
    retCode memCopy(computeMemory &dst, computeMemory &src, uint64 size,
                    uint64 dst_offset = 0, uint64 src_offset = 0,
                    int dst_queue = 0, int src_queue = 0,
                    void *callback_user_data = nullptr) override;

private:

    retCode cudaErrorFilter(int err);
    retCode attachDevice(int index);

};

} //alt::compute namespace

#endif // CUDADEVICE_H
