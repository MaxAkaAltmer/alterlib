#ifndef ASTREAMER_H
#define ASTREAMER_H

#include <alterlib/atypes.h>
#include <alterlib/at_array.h>
#include <alterlib/at_ring.h>
#include <alterlib/athread.h>
#include <alterlib/astring.h>
#include <alterlib/afile.h>

namespace alt
{

class streamer: public alt::delegateBase
{
public:
    streamer(const alt::string &fname, int buff_in=65535, int buff_out=65535);
    virtual ~streamer();

    void stop();
    void stop_async()
    {
        stop_flag=true;
    }
    bool stop_is_done()
    {
        return thread->isOff();
    }

    alt::string fileName(){return filename;}

    //управление потоком
    void moveTo(int64 pos);
    int64 fileSize();
    bool atEnd();
    void truncate();
    void toSleep();

    //процедуры чтения
    int read(void *data, int size);

    //процедуры записи
    int write(const void *data, int size);

private:

    int run(void *user);

    void userInitMode(int mode);

    enum workMode{
        MODE_SLEEP,
        MODE_SEEK,
        MODE_READ,
        MODE_WRITE,
        MODE_SIZE,
        MODE_TRUNCATE
    };

    volatile bool read_end;
    volatile int queryMode;
    volatile int currentMode;

    volatile bool stop_flag;
    alt::ring<uint8> ring_write;
    alt::ring<uint8> ring_read;
    alt::string filename;

    alt::thread *thread;

};

}

#endif // ASTREAMER_H
