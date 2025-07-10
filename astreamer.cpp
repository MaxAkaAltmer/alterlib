#include "astreamer.h"
#include <alterlib/afile.h>

using namespace alt;

streamer::streamer(const alt::string &fname, int buff_in, int buff_out)
    : filename(fname)
{
    if(buff_out<65536)buff_out=65536;
    if(buff_in<65536)buff_in=65536;
    ring_read.Resize(buff_out);
    ring_write.Resize(buff_in);
    stop_flag=false;
    read_end = false;
    queryMode = currentMode = MODE_SLEEP;

    thread = new alt::thread(alt::delegate<int,void*>(this,&streamer::run));
    thread->run(this);
}

streamer::~streamer()
{
    if(!stop_flag)
        stop();
    delete thread;
}

void streamer::userInitMode(int mode)
{
    if(mode==queryMode)return;
    if(queryMode==MODE_WRITE)
    {
        while(ring_write.Size())alt::sleep(1);
    }
    queryMode=MODE_SLEEP;
    while(queryMode!=currentMode)alt::sleep(1);
    ring_read.Free();
    queryMode=mode;
    if(mode==MODE_READ || mode==MODE_WRITE)
    {
        while(queryMode!=currentMode)alt::sleep(1);
    }
}

void streamer::stop()
{
    stop_flag=true;
    thread->wait();
}

void streamer::moveTo(int64 pos)
{
    userInitMode(MODE_SEEK);
    ring_write.WriteBlock((uint8*)&pos,sizeof(int64));
    while(queryMode!=currentMode)alt::sleep(1);
    userInitMode(MODE_SLEEP);
}

void streamer::truncate()
{
    userInitMode(MODE_TRUNCATE);
    while(queryMode!=currentMode)alt::sleep(1);
    userInitMode(MODE_SLEEP);
}

void streamer::toSleep()
{
    userInitMode(MODE_SLEEP);
}

int64 streamer::fileSize()
{
    userInitMode(MODE_SIZE);
    while(ring_read.Size()<int(sizeof(int64)))
        alt::sleep(1);
    int64 tmp;
    ring_read.Read((uint8*)&tmp,sizeof(int64));
    userInitMode(MODE_SLEEP);
    return tmp;
}

int streamer::read(void *data, int size)
{
    userInitMode(MODE_READ);
    int count=0;
    while(count<size)
    {
        int n=ring_read.blockSizeToRead();
        if(n)
        {
            if(n>size-count)n=size-count;
            memcpy(((uint8*)data)+count,ring_read.startPoint(),n);
            ring_read.Free(n);
            count+=n;
        }

        if(count<size && !ring_read.Size())
        {
            if(read_end)
            {
                if(!ring_read.Size())break;
                else continue;
            }
            alt::sleep(1);
        }
    }
    return count;
}

bool streamer::atEnd()
{
    if(read_end)
    {
        if(!ring_read.Size())return true;
    }
    return false;
}

int streamer::run(void *user)
{
    alt::file hand(filename);
    hand.open(alt::fileProto::OReadWrite|alt::fileProto::OAppend);

    while(!stop_flag)
    {        
        if(currentMode==MODE_WRITE && ring_write.Size())
        {
            int n=ring_write.blockSizeToRead();
            hand.write((char*)ring_write.startPoint(),n);
            ring_write.Free(n);
            continue;
        }

        if(currentMode!=queryMode)
        {
            if(queryMode==MODE_SEEK)
            {
                if(ring_write.Size()==sizeof(int64))
                {
                    int64 pos;
                    ring_write.Read((uint8*)&pos,sizeof(int64));
                    hand.seek(pos);
                }
                else
                {
                    alt::sleep(1);
                    continue;
                }
            }
            else if(queryMode==MODE_SIZE)
            {
                int64 size=hand.size();
                ring_read.WriteBlock((uint8*)&size,sizeof(int64));
            }
            else if(queryMode==MODE_TRUNCATE)
            {
                hand.resize(hand.pos());
                hand.seek(hand.size());
            }
            read_end = false;
            ring_write.Free();
            currentMode=queryMode;
        }

        if(currentMode==MODE_READ && ring_read.Allow() && !read_end)
        {
            int n=ring_read.blockSizeToWrite();
            int writed=hand.read((char*)ring_read.afterPoint(),n);
            if(writed>0)
            {
                ring_read.Added(writed);
                read_end = hand.atEnd();
            }
            else if(writed<0)
            {
                read_end=true;
            }
            else
            {
                read_end = hand.atEnd();
            }
            continue;
        }

        alt::sleep(1);
    }

    while(currentMode==MODE_WRITE && ring_write.Size())
    {
        int n=ring_write.blockSizeToRead();
        hand.write((char*)ring_write.startPoint(),n);
        ring_write.Free(n);
    }

    hand.close();

    return 0;
}

int streamer::write(const void *data, int size)
{
    userInitMode(MODE_WRITE);

    int count=0;
    while(count<size)
    {
        int n=ring_write.Allow();
        if(n>size-count)n=size-count;
        ring_write.WriteBlock((const uint8*)data+count,n);
        count+=n;
        if(count!=size)alt::sleep(1);
    }
    return size;
}

