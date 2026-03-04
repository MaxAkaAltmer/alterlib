#include "aprocess.h"

using namespace alt;

#if defined(linux) || defined(__APPLE__)

#include <stdlib.h>
#include <fcntl.h>      // Для O_* констант
#include <sys/mman.h>   // Для shm_open, mmap
#include <sys/stat.h>   // Для mode констант
#include <unistd.h>     // Для ftruncate, close

long long alt::processId()
{
    return getpid();
}

struct sharedInternal
{
    int shm_fd;
    bool host;
    string name;
};

static void* openSharedMemory(const string& name, uint8*& buffer, uintz size)
{

    bool host = false;

    int shm_fd = shm_open(name(), O_CREAT | O_EXCL | O_RDWR, 0666);

    if (shm_fd != -1)
    {
        host = true;
        if (ftruncate(shm_fd, size) == -1)
        {
            close(shm_fd);
            shm_unlink(name());
            return nullptr;
        }
    }
    else
    {
        shm_fd = shm_open(name(), O_RDWR, 0666);
        if (shm_fd == -1)
        {
            return nullptr;
        }
    }

    // Маппим память в адресное пространство
    uint8* shared_array = (uint8*)mmap(NULL, size,
                             PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_array == MAP_FAILED)
    {
        close(shm_fd);
        if (host)
            shm_unlink(name());
        return nullptr;
    }

    sharedInternal *hand = new sharedInternal;
    hand->shm_fd = shm_fd;
    hand->host = host;
    hand->name = name;
    buffer = shared_array;

    return hand;
}

static void closeSharedMemory(sharedInternal *hand, void *buffer, uintz size)
{
    // Очистка ресурсов
    munmap(buffer, size);
    close(hand->shm_fd);
    if(hand->host)
        shm_unlink(hand->name());
}

void processSharedMemory::cleanup(const string& name)
{
    shm_unlink(name());
}

#else
#include <windows.h>
#include <stdio.h>

long long alt::processId()
{
    return GetCurrentProcessId();
}

struct sharedInternal
{
    HANDLE hMapFile;
};

static void* openSharedMemory(const string& name, uint8*& buffer, uintz size)
{
    HANDLE hMapFile = CreateFileMappingA(
        INVALID_HANDLE_VALUE,    // Используем память, а не файл
        NULL,                    // Защита по умолчанию
        PAGE_READWRITE,          // Чтение/запись
        size>>32,                       // Размер (старшая часть)
        size,// Размер (младшая часть)
        name()          // Имя объекта
    );
    if (hMapFile == NULL) {
        return nullptr;
    }

    uint8* pArray = (uint8*)MapViewOfFile(
        hMapFile,                // Дескриптор объекта
        FILE_MAP_ALL_ACCESS,     // Доступ
        0, 0,                    // Смещение
        size // Размер
    );
    if (pArray == NULL) {
        CloseHandle(hMapFile);
        return nullptr;
    }

    sharedInternal *hand = new sharedInternal;
    hand->hMapFile = hMapFile;
    buffer = pArray;

    return hand;
}

static void closeSharedMemory(sharedInternal *hand, void *buffer, uintz size)
{
    UnmapViewOfFile(buffer);

    CloseHandle(hand->hMapFile);
}

void processSharedMemory::cleanup(const string& name)
{
    return;
}

#endif


processSharedMemory::processSharedMemory(const string& name, uintz size)
{
    internal = openSharedMemory(name,buffer,size);
    if(!internal)
        buffer = nullptr;
    else
        buffer_size = size;
}

processSharedMemory::~processSharedMemory()
{
    if(internal)
    {
        sharedInternal *hand = (sharedInternal*)internal;
        closeSharedMemory(hand,buffer, buffer_size);
        delete hand;
    }
}
