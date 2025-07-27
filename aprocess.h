#ifndef APROCESS_H
#define APROCESS_H

#include "atypes.h"
#include "astring.h"

namespace alt {

    long long processId();

    class processSharedMemory
    {
    public:
        processSharedMemory(const string& name, uintz size);
        processSharedMemory(const processSharedMemory&) = delete;
        ~processSharedMemory();

        processSharedMemory& operator = (const processSharedMemory&) = delete;

        const uint8& operator[](uintz ind) const
        {
            return buffer[ind];
        }

        const uint8* operator()() const
        {
            return buffer;
        }

        uintz size() const { return buffer_size; }

    private:

        uint8* buffer = nullptr;
        uintz buffer_size = 0;
        void *internal = nullptr;
    };

}

#endif // APROCESS_H
