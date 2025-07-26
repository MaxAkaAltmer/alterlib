/*****************************************************************************

This is part of Alterlib - the free code collection under the MIT License
------------------------------------------------------------------------------
Copyright (C) 2006-2023 Maxim L. Grishin  (altmer@arts-union.ru)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*****************************************************************************/

#include "afile.h"

#if defined(_MSC_VER)
    #include "external/dirent/include/dirent.h"
#else
    #include <unistd.h>
    #include <dirent.h>
    #include <stdio.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#if !defined(linux) && !defined(__APPLE__)
    #include <io.h>
#endif
#include <fcntl.h>

#if defined(_MSC_VER)
    /* Values for the second argument to access.
       These may be OR'd together.  */
    #define R_OK    4       /* Test for read permission.  */
    #define W_OK    2       /* Test for write permission.  */
    #define X_OK    R_OK    /* execute permission - unsupported in Windows,
                               use R_OK instead. */
    #define F_OK    0       /* Test for existence.  */

    #define wstat _wstat
    #define lseek64 _lseeki64
    #define stat _stat
    #define ftruncate64 _chsize_s
#endif

using namespace alt;

static string DriveTypeToString(DriveType t)
{
    switch (t) {
        case DriveType::Optical: return "Optical";
        case DriveType::Floppy: return "Floppy";
        case DriveType::Tape: return "Tape";
        case DriveType::RAM: return "RAM";
        case DriveType::Removable: return "Removable";
        case DriveType::Fixed: return "Fixed";
        default: return "Unknown";
    }
}

#include <iostream>
#include <iomanip>

static void print_full_toc(const byteArray& buffer)
{
    if (buffer.size() < sizeof(fullTocHeader)) {
        std::cout << "TOC buffer too small\n";
        return;
    }
    const fullTocHeader* header = reinterpret_cast<const fullTocHeader*>(buffer());
    int numDesc = (header->DataLength) / sizeof(fullTocDesc);
    const fullTocDesc* desc = reinterpret_cast<const fullTocDesc*>(buffer() + sizeof(fullTocHeader));

    std::cout << "Full TOC:\n";
    std::cout << "  First session: " << (int)header->FirstSession << "\n";
    std::cout << "  Last session:  " << (int)header->LastSession << "\n";
    std::cout << "  Descriptors:   " << numDesc << "\n\n";

    for (int i = 0; i < numDesc; ++i) {
        const fullTocDesc& d = desc[i];
        int control = d.Control_ADR & 0x0F;
        int adr = (d.Control_ADR >> 4) & 0x0F;

        std::cout << "Session: " << std::setw(2) << (int)d.SessionNumber
                  << "  TNO: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)d.TNO
                  << "  POINT: 0x" << std::setw(2) << (int)d.POINT
                  << "  Control: " << std::dec << control
                  << "  ADR: " << adr
                  << "  PMin: " << std::setw(2) << (int)d.PMin
                  << "  PSec: " << std::setw(2) << (int)d.PSec
                  << "  PFrame: " << std::setw(2) << (int)d.PFrame
                  << "\n";
    }
}

#if !defined(linux) && !defined(__APPLE__)
#include <winioctl.h>
#include <ntddcdrm.h> // или <ntddcdvd.h>
#include <ntddscsi.h>

#include <initguid.h>
#include <devguid.h>
#include <SetupAPI.h>
#include <string>
#include <iostream>
#include <iomanip>
#include <stdio.h>
#include <vector>
#include <map>

#pragma comment(lib, "SetupAPI.lib")

static string GetDeviceFriendlyName(const string& deviceInterfacePath) {
    // Получаем список устройств
    HDEVINFO hDevInfo = SetupDiCreateDeviceInfoList(nullptr, nullptr);
    if (hDevInfo == INVALID_HANDLE_VALUE)
        return {};

    // Открываем интерфейс устройства
    if (!SetupDiOpenDeviceInterfaceA(hDevInfo, deviceInterfacePath(), 0, nullptr)) {
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return {};
    }

    // Получаем SP_DEVINFO_DATA
    SP_DEVINFO_DATA devInfoData = {};
    devInfoData.cbSize = sizeof(devInfoData);
    if (!SetupDiEnumDeviceInfo(hDevInfo, 0, &devInfoData)) {
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return {};
    }

    // Пробуем получить Friendly Name
    char buffer[512];
    if (SetupDiGetDeviceRegistryPropertyA(
            hDevInfo,
            &devInfoData,
            SPDRP_FRIENDLYNAME,
            nullptr,
            (PBYTE)buffer,
            sizeof(buffer),
            nullptr)) {
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return buffer;
    }
    // Если нет Friendly Name, пробуем Device Description
    if (SetupDiGetDeviceRegistryPropertyA(
            hDevInfo,
            &devInfoData,
            SPDRP_DEVICEDESC,
            nullptr,
            (PBYTE)buffer,
            sizeof(buffer),
            nullptr)) {
        SetupDiDestroyDeviceInfoList(hDevInfo);
        return buffer;
    }

    SetupDiDestroyDeviceInfoList(hDevInfo);
    return string();
}

static DriveType GetDriveTypeFromDevice(const string& devicePath)
{
    HANDLE hDevice = CreateFileA(
        devicePath(),
        0,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );
    if (hDevice == INVALID_HANDLE_VALUE)
        return DriveType::Unknown;

    STORAGE_PROPERTY_QUERY query = { StorageDeviceProperty, PropertyStandardQuery };
    STORAGE_DEVICE_DESCRIPTOR* desc = (STORAGE_DEVICE_DESCRIPTOR*)malloc(1024);
    DWORD bytesReturned;
    DriveType result = DriveType::Unknown;

    if (DeviceIoControl(
            hDevice,
            IOCTL_STORAGE_QUERY_PROPERTY,
            &query,
            sizeof(query),
            desc,
            1024,
            &bytesReturned,
            nullptr)) {
        // Определяем тип

        /*
        switch (desc->BusType) {
        case BusTypeAtapi:
        case BusTypeScsi:
        ...
        }*/

        result = desc->RemovableMedia ? DriveType::Removable : DriveType::Fixed;

    }
    free(desc);
    CloseHandle(hDevice);

    return result;
}

static array<string> list_devices(const GUID* guid)
{
    array<string> rv;

    HDEVINFO hDevInfo = SetupDiGetClassDevs(guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (hDevInfo == INVALID_HANDLE_VALUE) return rv;

    SP_DEVICE_INTERFACE_DATA devInterfaceData;
    devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(hDevInfo, nullptr, guid, i, &devInterfaceData); ++i) {
        char devicePath[512];
        SP_DEVICE_INTERFACE_DETAIL_DATA_A *detailData = (SP_DEVICE_INTERFACE_DETAIL_DATA_A*)malloc(1024);
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
        DWORD requiredSize = 0;
        if (SetupDiGetDeviceInterfaceDetailA(hDevInfo, &devInterfaceData, detailData, 1024, &requiredSize, nullptr)) {
            rv.append(detailData->DevicePath);
        }
        free(detailData);
    }
    SetupDiDestroyDeviceInfoList(hDevInfo);
    return rv;
}

static string GetDeviceInterfacePathByDriveLetter(char driveLetter)
{
    // 1. Открываем том по букве
    std::string root = "\\\\.\\";
    root += driveLetter;
    root += ':';

    HANDLE hVolume = CreateFileA(
        root.c_str(),
        0,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );
    if (hVolume == INVALID_HANDLE_VALUE)
        return {};

    // 2. Получаем номер физического устройства
    STORAGE_DEVICE_NUMBER sdn = {0};
    DWORD bytesReturned = 0;
    BOOL ok = DeviceIoControl(
        hVolume,
        IOCTL_STORAGE_GET_DEVICE_NUMBER,
        nullptr,
        0,
        &sdn,
        sizeof(sdn),
        &bytesReturned,
        nullptr
    );
    CloseHandle(hVolume);
    if (!ok || (sdn.DeviceType != FILE_DEVICE_DISK && sdn.DeviceType != FILE_DEVICE_CD_ROM))
        return {};

    // 3. Перебираем все устройства через SetupAPI для дисков и CD-ROM
    const GUID* guids[] = { &GUID_DEVINTERFACE_DISK, &GUID_DEVINTERFACE_CDROM, &GUID_DEVINTERFACE_FLOPPY };

    for (int g = 0; g < 2; ++g) {
        HDEVINFO hDevInfo = SetupDiGetClassDevs(guids[g], nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
        if (hDevInfo == INVALID_HANDLE_VALUE)
            continue;

        SP_DEVICE_INTERFACE_DATA devInterfaceData;
        devInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

        for (DWORD i = 0; SetupDiEnumDeviceInterfaces(hDevInfo, nullptr, guids[g], i, &devInterfaceData); ++i) {
            DWORD requiredSize = 0;
            SetupDiGetDeviceInterfaceDetailA(hDevInfo, &devInterfaceData, nullptr, 0, &requiredSize, nullptr);
            if (requiredSize == 0)
                continue;

            SP_DEVICE_INTERFACE_DETAIL_DATA_A* detailData = (SP_DEVICE_INTERFACE_DETAIL_DATA_A*)malloc(requiredSize);
            if (!detailData)
                continue;
            detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

            if (SetupDiGetDeviceInterfaceDetailA(hDevInfo, &devInterfaceData, detailData, requiredSize, nullptr, nullptr)) {
                // 4. Открываем устройство
                HANDLE hDevice = CreateFileA(
                    detailData->DevicePath,
                    0,
                    FILE_SHARE_READ,
                    nullptr,
                    OPEN_EXISTING,
                    0,
                    nullptr
                );
                if (hDevice != INVALID_HANDLE_VALUE)
                {
                    STORAGE_DEVICE_NUMBER sdn2 = {0};
                    DWORD bytesReturned2 = 0;
                    if (DeviceIoControl(
                            hDevice,
                            IOCTL_STORAGE_GET_DEVICE_NUMBER,
                            nullptr,
                            0,
                            &sdn2,
                            sizeof(sdn2),
                            &bytesReturned2,
                            nullptr))
                    {
                        if (sdn2.DeviceType == sdn.DeviceType && sdn2.DeviceNumber == sdn.DeviceNumber)
                        {
                            string result = detailData->DevicePath;
                            CloseHandle(hDevice);
                            free(detailData);
                            SetupDiDestroyDeviceInfoList(hDevInfo);
                            return result;
                        }
                    }
                    CloseHandle(hDevice);
                }
            }
            free(detailData);
        }
        SetupDiDestroyDeviceInfoList(hDevInfo);
    }
    return string();
}

#define SPT_CDB_SIZE 12

static bool send_scsi_command(
    HANDLE hDevice,
    const std::vector<BYTE>& cdb,
    std::vector<BYTE>& dataBuffer,
    bool dataIn = true,
    DWORD timeout = 10 * 1000
) {
    SCSI_PASS_THROUGH_DIRECT sptd = { 0 };
    sptd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
    sptd.CdbLength = (UCHAR)cdb.size();
    sptd.DataIn = dataIn ? SCSI_IOCTL_DATA_IN : SCSI_IOCTL_DATA_OUT;
    sptd.DataTransferLength = (ULONG)dataBuffer.size();
    sptd.TimeOutValue = timeout / 1000;
    sptd.DataBuffer = dataBuffer.data();
    memcpy(sptd.Cdb, cdb.data(), cdb.size());

    DWORD returned = 0;
    BOOL ok = DeviceIoControl(
        hDevice,
        IOCTL_SCSI_PASS_THROUGH_DIRECT,
        &sptd, sizeof(sptd),
        &sptd, sizeof(sptd),
        &returned,
        nullptr
    );
    return ok;
}

static std::vector<BYTE> read_full_toc(HANDLE hDevice)
{
    std::vector<BYTE> cdb(SPT_CDB_SIZE, 0);
    cdb[0] = 0x43; // READ TOC/PMA/ATIP
    cdb[2] = 0x02; // Format = 0x02 (Full TOC)
    size_t allocLen = 2048; // С запасом
    cdb[7] = (allocLen >> 8) & 0xFF;
    cdb[8] = allocLen & 0xFF;

    std::vector<BYTE> buffer(allocLen, 0);
    if (!send_scsi_command(hDevice, cdb, buffer)) {
        std::cerr << "SCSI READ TOC (Full TOC) failed\n";
        return {};
    }
    // DataLength — первые 2 байта
    WORD dataLen = (buffer[0] << 8) | buffer[1];
    if (dataLen + 4 < buffer.size())
        buffer.resize(dataLen + 4);
    return buffer;
}

byteArray alt::storage_full_tok(const string &device, bool print)
{
    byteArray rv;

    HANDLE hDevice = CreateFileA(
        device(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );
    if (hDevice == INVALID_HANDLE_VALUE) {
        std::cerr << "Failed to open device\n";
        return rv;
    }

    auto toc = read_full_toc(hDevice);
    if (!toc.empty()) {
        rv.append(toc.data(),toc.size());
    } else {
        std::cerr << "Failed to read Full TOC\n";
    }

    CloseHandle(hDevice);

    if(print)
    {
        print_full_toc(rv);
    }

    return rv;
}

map<string,DriveType> alt::storages(bool print)
{
    map<string,DriveType> rv;

    // Оптические приводы
    array<string> list = list_devices(&GUID_DEVINTERFACE_CDROM);
    for(int i=0;i<list.size();i++)
    {
        rv[list[i]] = DriveType::Optical;
    }

    // Флоппи-диски
    list = list_devices(&GUID_DEVINTERFACE_FLOPPY);
    for(int i=0;i<list.size();i++)
    {
        rv[list[i]] = DriveType::Floppy;
    }

    // Ленточные накопители
    list = list_devices(&GUID_DEVINTERFACE_TAPE);
    for(int i=0;i<list.size();i++)
    {
        rv[list[i]] = DriveType::Tape;
    }

    // Диски (HDD/SSD/USB/SD)
    list = list_devices(&GUID_DEVINTERFACE_DISK);
    for(int i=0;i<list.size();i++)
    {
        rv[list[i]] = GetDriveTypeFromDevice(list[i]);
    }

    if(print)
    {
        for(int i=0;i<rv.size();i++)
        {
            std::cout << rv.key(i)() << " -> [" << GetDeviceFriendlyName(rv.key(i))() << "] " << DriveTypeToString(rv.value(i))() << std::endl;
        }
    }

    return rv;
}

string alt::storageOfPartition(const string &part)
{
    if(!part.size())
        return string();
    return GetDeviceInterfacePathByDriveLetter(part[0]);
}

map<string,DriveType> alt::partitions(bool print)
{
    map<string,DriveType> rv;

    DWORD drives = GetLogicalDrives();
    char root[] = "A:\\";
    for (int i = 0; i < 26; ++i) {
        if (drives & (1 << i)) {
            root[0] = 'A' + i;
            UINT type = GetDriveTypeA(root);
            switch (type) {
                case DRIVE_REMOVABLE:
                    rv[root] = DriveType::Removable;
                    break;
                case DRIVE_FIXED:
                    rv[root] = DriveType::Fixed;
                    break;
                case DRIVE_CDROM:
                    rv[root] = DriveType::Optical;
                    break;
                case DRIVE_RAMDISK:
                    rv[root] = DriveType::RAM;;
                    break;
                default:
                    rv[root] = DriveType::Unknown;;
            }
        }
    }

    if(print)
    {
        for(int i=0;i<rv.size();i++)
        {
            std::cout << rv.key(i)() << " -> [" << GetDeviceInterfacePathByDriveLetter(rv.key(i)()[0])() << "] " << DriveTypeToString(rv.value(i))() << std::endl;
        }
    }

    return rv;
}

#else
#include <string>
#include <fstream>
#include <sys/stat.h>
#include <filesystem>
#include <sys/ioctl.h>
#include <scsi/sg.h>
#include <linux/cdrom.h>

DriveType GetDriveTypeFromSysfs(const std::string& devname) {
    std::string sysbase = "/sys/class/block/" + devname;

    // 1. SCSI type
    std::ifstream ftype(sysbase + "/device/type");
    if (ftype) {
        int t = -1;
        ftype >> t;
        switch (t) {
            case 0: // Direct-access (HDD/SSD/USB/SD)
                break; // определим ниже
            case 1: return DriveType::Tape;
            case 4: return DriveType::Floppy;
            case 5: return DriveType::Optical;
            default: return DriveType::Unknown;
        }
    }

    // 2. RAM-диск (нет device-ссылки)
    struct stat st;
    if (stat((sysbase + "/device").c_str(), &st) != 0)
        return DriveType::RAM;

    // 3. Removable
    std::ifstream frem(sysbase + "/removable");
    if (frem) {
        int r = 0;
        frem >> r;
        if (r == 1)
            return DriveType::Removable;
    }

    // 4. Fixed
    return DriveType::Fixed;
}

map<string,DriveType> alt::storages(bool print)
{
    map<string, DriveType> rv;

    for (const auto& entry : std::filesystem::directory_iterator("/sys/class/block"))
    {
        std::string name = entry.path().filename();

        if (!std::filesystem::exists(entry.path() / "partition"))
        {
            DriveType type = GetDriveTypeFromSysfs(name);
            rv[name.c_str()] = type;
        }
    }

    if(print)
    {
        for(int i=0;i<rv.size();i++)
        {
            std::cout << rv.key(i)() << " -> " << DriveTypeToString(rv.value(i))() << std::endl;
        }
    }

    return rv;
}

string alt::storageOfPartition(const string &part)
{
    try
    {
        std::string partname = "/sys/class/block/";
        partname += part();
        std::filesystem::path path = std::filesystem::canonical(partname);
        std::string full = path.string();
        auto pos = full.find_last_of('/');
        if (pos != std::string::npos)
        {
            std::string device_path = full.substr(0, pos);
            return std::filesystem::path(device_path).filename().c_str();
        }
    }
    catch (...)
    {
        return "";
    }
    return "";
}

map<string,DriveType> alt::partitions(bool print)
{
    map<string, DriveType> rv;
    auto devices = storages();  // Предварительно получаем устройства и их типы

    for (const auto& entry : std::filesystem::directory_iterator("/sys/class/block"))
    {
        std::string partname = entry.path().filename();

        if (std::filesystem::exists(entry.path() / "partition"))
        {
            string parent = storageOfPartition(partname.c_str());
            if (!parent.isEmpty() && devices.contains(parent))
            {
                rv[partname.c_str()] = devices[parent];
            }
            else
            {
                rv[partname.c_str()] = DriveType::Unknown;
            }
        }
    }

    if(print)
    {
        for(int i=0;i<rv.size();i++)
        {
            std::cout << rv.key(i)() << " -> [" << storageOfPartition(rv.key(i)())() << "] " << DriveTypeToString(rv.value(i))() << std::endl;
        }
    }

    return rv;
}

enum class DeviceAccessCheckResult {
    OK,               // Всё в порядке, устройство есть, доступ разрешён
    NotFound,         // Устройство не существует
    NotCDROM,         // Устройство не является CD/DVD
    NoPermissions,    // Недостаточно прав (без root и без групп cdrom/sg)
    IoctlFailed       // Сломан SG_IO или не распознано устройство
};

DeviceAccessCheckResult check_cdrom_device_access(const std::string& device)
{
    struct stat st;
    if (stat(device.c_str(), &st) != 0)
    {
        // Файл не существует
        return DeviceAccessCheckResult::NotFound;
    }

    // Проверим, что это блочное/символьное устройство
    if (!S_ISCHR(st.st_mode) && !S_ISBLK(st.st_mode))
    {
        return DeviceAccessCheckResult::NotCDROM;
    }

    // Пробуем открыть
    int fd = open(device.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0)
    {
        if (errno == EACCES || errno == EPERM)
        {
            return DeviceAccessCheckResult::NoPermissions;
        }
        else
        {
            return DeviceAccessCheckResult::NotFound;
        }
    }

    // Проверка: действительно ли это CD/DVD (через ioctl)
    struct cdrom_tochdr hdr;
    if (ioctl(fd, CDROMREADTOCHDR, &hdr) < 0)
    {
        // Это не CD/DVD устройство, или нет диска
        close(fd);
        return DeviceAccessCheckResult::NotCDROM;
    }

    // Дополнительно: проверим SG_IO на этом устройстве
    uint8_t cdb[6] = { 0x00, 0, 0, 0, 0, 0 }; // TEST UNIT READY
    uint8_t sense[32] = {};
    sg_io_hdr_t io_hdr = {};
    io_hdr.interface_id = 'S';
    io_hdr.cmdp = cdb;
    io_hdr.cmd_len = sizeof(cdb);
    io_hdr.dxfer_direction = SG_DXFER_NONE;
    io_hdr.sbp = sense;
    io_hdr.mx_sb_len = sizeof(sense);
    io_hdr.timeout = 1000;

    if (ioctl(fd, SG_IO, &io_hdr) < 0)
    {
        close(fd);
        return DeviceAccessCheckResult::IoctlFailed;
    }

    close(fd);
    return DeviceAccessCheckResult::OK;
}

bool send_scsi_read_toc(int fd, byteArray& buffer)
{
    size_t allocLen = 2048;
    buffer.resize(allocLen);
    buffer.fill(0);

    uint8_t cdb[10] = {};
    cdb[0] = 0x43;   // READ TOC
    cdb[1] = 0x02;   // MSF = 0, Format = 0x02 (Full TOC)
    cdb[7] = (allocLen >> 8) & 0xFF;
    cdb[8] = allocLen & 0xFF;

    sg_io_hdr_t io_hdr = {};
    io_hdr.interface_id = 'S';
    io_hdr.cmdp = cdb;
    io_hdr.cmd_len = sizeof(cdb);
    io_hdr.dxferp = buffer();
    io_hdr.dxfer_len = allocLen;
    io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr.timeout = 5000;

    if (ioctl(fd, SG_IO, &io_hdr) < 0)
    {
        perror("SG_IO ioctl failed");
        return false;
    }

    if ((io_hdr.info & SG_INFO_OK_MASK) != SG_INFO_OK)
    {
        std::cerr << "SCSI READ TOC command failed\n";
        return false;
    }

    // Обрезаем по фактической длине (префикс + DataLength)
    uint16_t dataLen = (buffer[0] << 8) | buffer[1];
    size_t totalLen = dataLen + 2;
    if (totalLen > allocLen)
        totalLen = allocLen;

    buffer.resize(totalLen);
    return true;
}

byteArray storage_full_tok(const string &device, bool print)
{
    int fd = open(device(), O_RDONLY | O_NONBLOCK);
    if (fd < 0)
    {
        perror("Failed to open CD/DVD device");
        return byteArray();
    }

    byteArray toc;
    if (!send_scsi_read_toc(fd, toc))
    {
        std::cerr << "Failed to read Full TOC\n";
        close(fd);
        return byteArray();
    }

    if (print)
    {
        print_full_toc(toc);
    }

    close(fd);
    return toc;
}


#endif

void pathParcer::setPath(const string &path)
{
    char sep=_defSep;
    for(int i=0;i<path.size();i++)
    {
        if(path[i]=='/' || path[i]=='\\')
        {
            sep=path[i];
            break;
        }
    }

    array<string> list=split(path);
    array<string> rez;
    if(list.size())
    {
        rez.append(list[0]);
    }
    for(int i=1;i<list.size();i++)
    {
        if(list[i]=="..")
        {
            if(rez.size() && rez.last()!="..")
            {
                rez.pop();
                continue;
            }
            else
            {
                rez.append(list[i]);
            }
        }
        else if(list[i]==".")
        {
            if(rez.size())
            {
                continue;
            }
            else
            {
                rez.append(list[i]);
            }
        }
        else if(list[i].isEmpty())
        {
            continue;
        }
        else
        {
            rez.append(list[i]);
        }
    }
    _path=string::join(rez,sep);
}

array<string> pathParcer::split(const string &path, bool scip_empty)
{
    array<string> rv;
    string tmp;
    for(int i=0;i<path.size();i++)
    {
        if(path[i]=='/' || path[i]=='\\')
        {
            if(!scip_empty || !tmp.isEmpty())
                rv.append(tmp);
            tmp.clear();
        }
        else
        {
            tmp.append(path[i]);
        }
    }
    if(!tmp.isEmpty())rv.append(tmp);
    return rv;
}

string pathParcer::getExtension()
{
    for(int i=_path.size();i>=0;i--)
    {
        if(_path[i]=='/' || _path[i]=='\\')return string();
        if(_path[i]=='.')return _path.right(i+1);
    }
    return string();
}

string pathParcer::getName()
{
    for(int i=_path.size();i>=0;i--)
    {
        if(_path[i]=='/' || _path[i]=='\\')return _path.right(i+1);
    }
    return _path;
}

string pathParcer::getBaseName()
{
    string tmp=getName();
    for(int i=0;i<tmp.size();i++)
    {
        if(tmp[i]=='.')return tmp.left(i);
    }
    return tmp;
}

string pathParcer::getNameNoExt()
{
    string tmp=getName();
    for(int i=tmp.size();i>=0;i--)
    {
        if(tmp[i]=='.')return tmp.left(i);
    }
    return tmp;
}

string pathParcer::getDirectory()
{
    for(int i=_path.size();i>=0;i--)
    {
        if(_path[i]=='/' || _path[i]=='\\')return _path.left(i);
    }
    return string();
}

string pathParcer::createAbsolutePath(string to, bool this_is_dir)
{
    array<string> curr = split(_path,true);
    array<string> dest = split(to,true);
    int ind=0;

    if(to[0]!='.')
        return to;

    if(!this_is_dir)
        curr.pop();

    curr.append(dest);

    for(int i=0; i<curr.size(); i++)
    {
        if(curr[i]==".")
        {
            curr.cut(i);
            i++;
        }
        else if(curr[i]=="..")
        {
            if(!i) return "";
            curr.cut(i-1,2);
            i-=2;
        }
    }
    return string::join(curr,_defSep);
}

string pathParcer::createRelativePath(string to, bool this_is_dir)
{
    array<string> curr = split(_path,true);
    array<string> dest = split(to,true);
    int ind=0;

    if(!curr.size())
        return to;

    if(!this_is_dir)
        curr.pop();

    for(;ind<curr.size() && ind<dest.size();ind++)
    {
        if(curr[ind]!=dest[ind])
            break;
    }

    if(ind==0)
        return to;

    string rv = ".";
    rv.append(_defSep);
    if(ind < curr.size())
    {
        rv.clear();
        for(int i = ind; i<curr.size(); i++)
        {
            rv += "..";
            rv.append(_defSep);
        }
    }
    for(int i = ind; i<dest.size(); i++)
    {
        rv+=dest[i];
    }
    return rv;
}

bool dirIsWriteble(string path)
{
    if(path.last()!='/')path+="/";
    int testInd=0;
    string fname=path+"test.txt";
    while(file::exists(fname))
    {
        fname=path+"test"+string::fromInt(testInd++)+".txt";
    }
    file hand(fname);
    if(hand.open(file::OWriteOnly))
    {
        if(hand.write(fname(),fname.size())==fname.size())
        {
            hand.close();
            if(hand.open(file::OReadOnly))
            {
                if(hand.readText()==fname)
                {
                    hand.close();
                    file::remove(fname);
                    return true;
                }
            }
        }
        hand.close();
        file::remove(fname);
    }
    return false;
}

array<string> dirEntryList(const string &path, alt::set<string> extFilter, int type)
{
    array<string> rv;
#if !defined(linux) && !defined(__APPLE__)
    array<wchar_t> upath=path.toUnicode();
    _WDIR *dir;
    struct _wdirent *drnt;
    dir = _wopendir(upath());
    while (dir && (drnt = _wreaddir(dir)) != NULL)
    {
        string node=string::fromUnicode(drnt->d_name);
        if(node=="." || node=="..")continue;
        if(type)
        {
            struct stat _Stat;
            array<wchar_t> sub_name=(path+"/"+node).toUnicode();
            wstat(sub_name(),&_Stat);
            if(type<0 && S_ISREG(_Stat.st_mode))continue;
            if(type>0 && !S_ISREG(_Stat.st_mode))continue;
        }
        if(extFilter.size())
        {
            int ind = node.findBackChar('.');
            if(ind<0)continue;
            string ext=node.right(ind+1).toLower();
            if(!extFilter.contains(ext))continue;
        }
        rv.append(node);
    }
#else
    DIR *dir;
    struct dirent *drnt;
    dir = opendir(path());
    while (dir && (drnt = readdir(dir)) != NULL)
    {
        string node(drnt->d_name);
        if(node=="." || node=="..")continue;
        if(type)
        {
            if(type>0 && drnt->d_type!=DT_REG)continue;
            else if(type<0 && drnt->d_type==DT_REG)continue;
        }
        if(extFilter.size())
        {
            int ind = node.findBackChar('.');
            if(ind<0)continue;
            string ext=node.right(ind+1).toLower();
            if(!extFilter.contains(ext))continue;
        }
        rv.append(node);
    }
#endif
    return rv;
}

file::file()
{
    handler = -1;
}

file::file(const string &name)
{
    fname = name;
    handler = -1;
}

file::~file()
{
    close();
}

bool file::setFileName(const string &name)
{
    if(open_flags)return false;
    fname=name;
    return true;
}

void file::close()
{
    if(handler>=0)
    {
        ::close(handler);
        handler = -1;
    }
    fileProto::close();
}

bool file::open(int flags)
{
    if(fname.isEmpty())return false;
    if(!flags)return false;

    int oflags=0;
#ifdef O_LARGEFILE
    oflags|=O_LARGEFILE;
#endif
#ifdef O_BINARY
    oflags|=O_BINARY;
#endif
    if((flags&OReadOnly) && (flags&OWriteOnly))oflags|=O_RDWR;
    else if(flags&OReadOnly)oflags|=O_RDONLY;
    else if(flags&OWriteOnly)oflags|=O_WRONLY;
    if(flags&OAppend)oflags|=O_APPEND;
    if(flags&OTruncate)oflags|=O_TRUNC;

#if !defined(linux) && !defined(__APPLE__)
    array<wchar_t> wname=fname.toUnicode();
    int hand=::_wopen(wname(),oflags);

    if(hand<0 && (flags&OWriteOnly))
    {
#ifdef _MSC_VER
        int pmode = 0;
        if((flags&OReadOnly) && (flags&OWriteOnly))
            pmode|=_S_IREAD|_S_IWRITE;
        else if(flags&OReadOnly)
            return false;
        else if(flags&OWriteOnly)
            pmode|=_S_IWRITE;
        hand=::_wcreat(wname(),pmode);
#else
        hand=::_wcreat(wname(),00666);
#endif
        if(hand>=0)
        {
            ::close(hand);
            hand=::_wopen(wname(),oflags);
        }
    }
#else
    int hand=::open(fname(),oflags);

    if(hand<0 && (flags&OWriteOnly))
    {
        hand=::creat(fname(),00666);
        if(hand>=0)
        {
            ::close(hand);
            hand=::open(fname(),oflags);
        }
    }
#endif

    if(hand<0)return false;

    open_flags = flags;
    handler=hand;

    return true;
}

int64 file::size() const
{
    if(handler<0)return -1;

    int64 oldpos=pos();
#if !defined(__APPLE__)
    ::lseek64(handler,0,SEEK_END);
#else
    ::lseek(handler,0,SEEK_END);
#endif
    int64 rv=pos();
#if !defined(__APPLE__)
    ::lseek64(handler,oldpos,SEEK_SET);
#else
    ::lseek(handler,oldpos,SEEK_SET);
#endif
    return rv;
}

bool file::seek(int64 pos)
{
    if(handler<0)return false;
#if !defined(__APPLE__)
    return ::lseek64(handler,pos,SEEK_SET)==pos;
#else
    return ::lseek(handler,pos,SEEK_SET)==pos;
#endif
}

int64 file::pos() const
{
    if(handler<0)return -1;
#if !defined(__APPLE__)
    return ::lseek64(handler,0,SEEK_CUR);
#else
    return ::lseek(handler,0,SEEK_CUR);
#endif
}

int file::read_hand(void *buff, int size)
{
    if(handler<0)return -1;
    if(size<=0)return 0;
    int rv = ::read(handler,buff,size);

    return rv;
}

int file::write_hand(const void *buff, int size)
{
    if(handler<0)return -1;
    if(size<=0)return 0;
    return ::write(handler,buff,size);
}

bool file::resize(int64 size)
{
    if(handler<0)return false;
#if !defined(__APPLE__)
    if (::ftruncate64(handler, size) != 0) return false;
#else
    if (::ftruncate(handler, size) != 0) return false;
#endif
    return true;
}

bool file::isSequential() const
{
    return false;
}

bool file::exists(string fname)
{
    if(fname.isEmpty())return false;
#if !defined(linux) && !defined(__APPLE__)
    array<wchar_t> wname=fname.toUnicode();
    if(_waccess(wname(),F_OK)==-1)return false;
#else
    if(access(fname(),F_OK)==-1)return false;
#endif
    return true;
}


bool file::remove(string fname)
{
    if(fname.isEmpty())return false;
#if !defined(linux) && !defined(__APPLE__)
    array<wchar_t> wname=fname.toUnicode();
    if(_wunlink(wname())==-1)return false;
#else
    if(unlink(fname())==-1)return false;
#endif
    return true;
}

bool file::rename(string before, string after)
{
#if !defined(linux) && !defined(__APPLE__)
    array<wchar_t> bname=before.toUnicode();
    array<wchar_t> aname=after.toUnicode();
    if(!(_wrename(bname(),aname())))return true;
#else
    if(!(::rename(before(),after())))return true;
#endif
    return false;
}

alt::time file::changeTime(string fname)
{
    struct stat t_stat;
#if !defined(linux) && !defined(__APPLE__)
    array<wchar_t> wname=fname.toUnicode();
    wstat(wname(), &t_stat);
#else
    stat(fname(), &t_stat);
#endif
    if(sizeof(t_stat.st_mtime)==8)
        return alt::time(((uint64)t_stat.st_mtime)*(uint64)1000000);
    return alt::time(((uint32)t_stat.st_mtime)*(uint64)1000000);
}

bool file::replicate(string src, string dst)
{
    file hsrc(src);
    file hdst(dst);
    if(!hsrc.open())return false;
    if(!hdst.create())return false;

    int64 total=hsrc.size();
    if(hdst.copy(&hsrc,total)!=total)return false;
    return true;
}
