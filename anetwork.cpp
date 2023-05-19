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

#include "anetwork.h"
#include "athread.h"
#include "atime.h"
#if !defined(__linux) && !defined(__APPLE__)
    #include <winsock2.h>
    #include <process.h>
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <sys/ioctl.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <memory.h>
    #include <errno.h>

    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define SD_BOTH SHUT_RDWR

    #define WSAEINPROGRESS EINPROGRESS
    #define WSAEWOULDBLOCK EWOULDBLOCK
    #define WSAENOBUFS ENOBUFS
    #define WSAEMSGSIZE EMSGSIZE

    #define ioctlsocket ioctl
    #define closesocket close

static int WSAGetLastError()
{
    return errno;
}

#endif

using namespace alt;

//////////////////////////////////////////////////////////////////////////
//разбор ошибок

static retCode errFailInitLib(){return retCode(-1);}
static retCode errInvalideIP(){return retCode(-2);}
static retCode errInvalideTransportType(){return retCode(-3);}
static retCode errSocketCreate(){return retCode(-4);}
static retCode errConnect(){return retCode(-5);}
static retCode errFailOnRead(){return retCode(-6);}
static retCode errFailOnWrite(){return retCode(-7);}
static retCode errSelect(){return retCode(-8);}
static retCode errSocketUnblock(){return retCode(-9);}
static retCode errSocketBind(){return retCode(-10);}
static retCode errSocketListen(){return retCode(-11);}

string connection::errorDescriptor(retCode code)
{
    switch(code.get())
    {
    case -1: return "Fail to init socket library.";
    case -2: return "Invalide IP.";
    case -3: return "Invalide transport type.";
    case -4: return "Fail to create socket.";
    case -5: return "Fail connect.";
    case -6: return "Fail on read.";
    case -7: return "Fail on write.";
    case -8: return "Fail on select.";
    case -9: return "Fail to make unblocked.";
    case -10: return "Fail to bind.";
    case -11: return "Fail to listen.";
    case -1001: return "Timeout.";
    case -1002: return "Ping response incorrect.";
    };
    if(code.error())return "Unknown error.";
    return "Ok.";
}

//////////////////////////////////////////////////////////////////////////
//Внутреннняя структура сёкета
struct ASocketInternal
{    
    SOCKET          sock;
    sockaddr_in     addr;
    retCode    last_error;
    int initLevel;
    bool blocking;
};

//////////////////////////////////////////////////////////////////////////
//Нативная реализация сёкета
connection::connection(TransportType type, const string &host, int port, bool blocking)
{    
    internal = new ASocketInternal;
    _type=type;
    _host=host;
    _port=port;

    ASocketInternal *hand=(ASocketInternal*)internal;
    hand->initLevel=0;
    hand->last_error=0;
    hand->blocking=blocking;

    //инициализация библиотеки
#if !defined(__linux) && !defined(__APPLE__)
    WSADATA WsaData;
    int err = WSAStartup(MAKEWORD(1,1), &WsaData);

    if (err != 0)
    {
        hand->last_error=errFailInitLib();
        return;
    }
#endif
    hand->initLevel=1;    
}


connection::~connection()
{
    ASocketInternal *hand=(ASocketInternal*)internal;

    disconnect();
#if !defined(__linux) && !defined(__APPLE__)
    if(hand->initLevel>0)
    {
        WSACleanup();
    }
#endif
    delete hand;    
}

alt::connection& connection::setOption(OptionValue opt, const variant &val)
{
    ASocketInternal *hand=(ASocketInternal*)internal;

    bool ok;
    int temp=val.toInt(&ok);
    if(!ok)return *this;

    if(hand->initLevel>1)
    {
        if(opt==ORecvBuffSize)
            setsockopt(hand->sock,SOL_SOCKET,SO_RCVBUF,(char*)&temp,sizeof(int));
        else if(opt==OSendBuffSize)
            setsockopt(hand->sock,SOL_SOCKET,SO_SNDBUF,(char*)&temp,sizeof(int));
        else if(opt==OKeepAlive)
            setsockopt(hand->sock,SOL_SOCKET,SO_KEEPALIVE,(char*)&temp,sizeof(int));
        else if(_type==TCP && opt==ONoDelay)
            setsockopt(hand->sock,IPPROTO_TCP,TCP_NODELAY,(char*)&temp,sizeof(int));
    }
    options[opt]=val;

    return *this;
}


retCode connection::connect()
{
    disconnect();

    ASocketInternal *hand=(ASocketInternal*)internal;
    if(!hand->initLevel)return hand->last_error;
    hand->last_error=0;

    //подготовка адреса
    memset(&hand->addr,0,sizeof(hand->addr));
    hand->addr.sin_family=AF_INET;
    hand->addr.sin_port=htons(_port);
#if !defined(__linux) && !defined(__APPLE__)
    hand->addr.sin_addr.S_un.S_addr=inet_addr(toIP(_host)());

    if(INADDR_NONE==hand->addr.sin_addr.S_un.S_addr)
#else
    hand->addr.sin_addr.s_addr=inet_addr(toIP(_host)());

    if(INADDR_NONE==hand->addr.sin_addr.s_addr)
#endif
    {
        hand->last_error=errInvalideIP();
        return hand->last_error;
    }

    //создание сёкета
    switch(_type)
    {
    case connection::UDP:
        hand->sock=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        break;
    case connection::TCP:
        hand->sock=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        break;
    case connection::ICMP:
        hand->sock=socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
        break;
    default:
        hand->last_error=errInvalideTransportType();
        return hand->last_error;
    }

    if(hand->sock==INVALID_SOCKET)
    {
        hand->last_error=errSocketCreate();
        return hand->last_error;
    }
    hand->initLevel=2;

    if(!hand->blocking)
    {
        unsigned long tmp=1;
        if(ioctlsocket(hand->sock,FIONBIO,&tmp)==SOCKET_ERROR)
        {
            disconnect();
            hand->last_error=errSocketUnblock();
            return hand->last_error;
        }
    }

    for(int i=0;i<options.size();i++)
    {
        setOption(options.key(i),options.value(i));
    }

    if(::connect(hand->sock,(sockaddr*)&hand->addr,sizeof(hand->addr))==SOCKET_ERROR)
    {
        int err=WSAGetLastError();
        if((err==WSAEINPROGRESS || err==WSAEWOULDBLOCK) && !hand->blocking)
        {
            hand->initLevel=3;
            return 0;
        }
        disconnect();
        hand->last_error=errConnect();
        return hand->last_error;
    }
    else
    {
        hand->initLevel=4;
    }

    return 1;
}

retCode connection::status()
{
    ASocketInternal *hand=(ASocketInternal*)internal;
    if(hand->initLevel<3)return hand->last_error;
    if(hand->initLevel>3)return 1;

    fd_set w,e;
    FD_ZERO(&w);
    FD_ZERO(&e);
    FD_SET(hand->sock,&w);
    FD_SET(hand->sock,&e);

    timeval delay;
    delay.tv_sec=0;
    delay.tv_usec=0;

    if(select(0,NULL,&w,&e,&delay)==SOCKET_ERROR)
    {
        disconnect();
        hand->last_error=errSelect();
        return hand->last_error;
    }
    if(FD_ISSET(hand->sock, &w))
    {
        hand->initLevel=4;
    }

    if(hand->initLevel==3)return 0;
    return 1;
}

retCode connection::lastError()
{
    ASocketInternal *hand=(ASocketInternal*)internal;
    return hand->last_error;
}

retCode connection::disconnect()
{
    ASocketInternal *hand=(ASocketInternal*)internal;

    if(hand->initLevel<1)return hand->last_error;

    if(hand->initLevel>2)
    {
        shutdown(hand->sock,SD_BOTH);
        hand->initLevel=2;
    }
    if(hand->initLevel>1)
    {
        closesocket(hand->sock);
        hand->initLevel=1;
        return 1;
    }
    return 0;
}

retCode connection::send(const void *data, int size)
{
    ASocketInternal *hand=(ASocketInternal*)internal;
    retCode stat=status();
    if(stat.value())
    {
        int rval=::send(hand->sock,(char*)data,size,0);
        if(rval==SOCKET_ERROR)
        {
            rval=WSAGetLastError();
            if(!hand->blocking)
            {
                if(_type==TCP && (rval==WSAENOBUFS || rval==WSAEWOULDBLOCK))return 0;
                else if(/*_type==UDP &&*/ rval==WSAEWOULDBLOCK)return 0;
            }
            else
            {
                if(_type==TCP && rval==WSAENOBUFS)return 0;
            }
            disconnect();
            hand->last_error=errFailOnWrite();
            return hand->last_error;
        }
        return rval;
    }
    if(!stat.error())return 0;
    return hand->last_error;
}

retCode connection::sendAll(const void *data, int size, int timeout_us)
{
    ASocketInternal *hand=(ASocketInternal*)internal;
    int count=size;
    const uint8 *buff=(const uint8*)data;
    uint64 stamp=time::uStamp();
    do
    {
        retCode code=send(buff,count);
        if(code.error())return code;
        count-=code.get();
        buff+=code.get();
        if(count)
        {
            if(timeout_us>0)
            {
                uint64 curr=time::uStamp();
                if(curr-stamp>(uint64)timeout_us)
                {
                    hand->last_error=-1001;
                    return hand->last_error;
                }
            }
            sleep(0);
        }
    }while(count);
    return size;
}

retCode connection::waitData(int time_ms)
{
    ASocketInternal *hand=(ASocketInternal*)internal;
    retCode stat=status();
    if(stat.value())
    {
        fd_set r;
        FD_ZERO(&r);
        FD_SET(hand->sock,&r);

        timeval delay;
        delay.tv_sec=time_ms/1000;
        delay.tv_usec=(time_ms%1000)*1000;

        if(select(0,&r,NULL,NULL,&delay)==SOCKET_ERROR)
        {
            disconnect();
            hand->last_error=errFailOnRead();
            return hand->last_error;
        }
        if(FD_ISSET(hand->sock, &r))
        {
            return 1;
        }
    }
    if(!stat.error())return 0;
    return hand->last_error;
}

retCode connection::recv(void *data, int size)
{
    ASocketInternal *hand=(ASocketInternal*)internal;
    retCode stat=status();
    if(stat.value())
    {
        int rval=::recv(hand->sock,(char*)data,size,0);
        if(rval==SOCKET_ERROR)
        {
                rval=WSAGetLastError();
                if(!hand->blocking)
                {
                    if(_type==TCP && (rval==WSAEWOULDBLOCK || rval==WSAEMSGSIZE))return 0;
                    if(/*_type==UDP &&*/ rval==WSAEWOULDBLOCK)return 0;
                }
                else
                {
                    if(_type==TCP && rval==WSAEMSGSIZE)return 0;
                }
                disconnect();
                hand->last_error=errFailOnRead();
                return hand->last_error;
        }
        return rval;
    }
    if(!stat.error())return 0;
    return hand->last_error;
}

retCode connection::recvAll(void *data, int size, int timeout_us, int minimal_size)
{
    ASocketInternal *hand=(ASocketInternal*)internal;
    int count=minimal_size<0?size:minimal_size, buff_size=size;
    uint8 *buff=(uint8*)data;
    uint64 stamp=time::uStamp();
    do
    {
        retCode code=recv(buff,buff_size);
        if(code.error())return code;
        count-=code.get();
        buff_size-=code.get();
        buff+=code.get();
        if(count>0)
        {
            if(timeout_us>0)
            {
                uint64 curr=time::uStamp();
                if(curr-stamp>(uint64)timeout_us)
                {
                    hand->last_error=-1001;
                    return hand->last_error;
                }
            }
            sleep(0);
        }
    }while(count>0);
    return size-buff_size;
}

bool connection::isIP(const string &addr)
{
    if(inet_addr(addr())==INADDR_NONE)return false;
    return true;
}

string connection::toIP(const string &addr)
{
    if(!isIP(addr))
    {
        struct hostent *ip=gethostbyname(addr());
        if(!ip || ip->h_addrtype != AF_INET)
        {
            return string();
        }
        return inet_ntoa(*(struct in_addr *) ip->h_addr_list[0]);
    }
    return addr;
}

struct ICMPHeader
{
    uint8 type;
    uint8 code;
    uint16 checksum;
    uint16 id;
    uint16 seq;
    uint32 data[1];
};

#ifndef ICMP_ECHO
    #define ICMP_ECHO		8	/* Echo Request			*/
    #define ICMP_ECHOREPLY		0	/* Echo Reply			*/
#endif

unsigned short in_check(void* buf, size_t len){

    int val = 0;
    unsigned short *sp = (unsigned short*)buf;

    while( len >= 2 ) {
        val += *sp++;
        len -= 2;
    }

    if( len == 1 ) {
        val += *(unsigned char *)sp;
    }

    val = (val >> 16) + (val & 0xffff);
    val += (val >> 16);

    return ~(unsigned short)val;
}

retCode connection::ping(string addr, int delay_us)
{
#if !defined(__linux) && !defined(__APPLE__)
    WSADATA WsaData;
    int err = WSAStartup(MAKEWORD(1,1), &WsaData);
    if (err != 0)return -1;
#endif

    addr=toIP(addr);
    if(addr.isEmpty())
    {
#if !defined(__linux) && !defined(__APPLE__)
        WSACleanup();
#endif
        return -2;
    }
    uint8 buffrd[1024]={0},buff[1024]={0};
    struct ICMPHeader *icmp=(ICMPHeader*)buff;

    icmp->type = ICMP_ECHO;
    icmp->code = 0;
#if !defined(__linux) && !defined(__APPLE__)
    icmp->id = _getpid();
#else
    icmp->id = getpid();
#endif
    icmp->seq = 1;

    int len = 64;
    icmp->checksum = in_check(icmp, len);

    retCode rc;
    connection sock(connection::ICMP,addr,0,false);
    if(!(rc=sock.connect()).error())
    {
        uint64 start=time::uStamp();
        if(!(rc=sock.sendAll(icmp,len,delay_us)).error())
        {
            if(!(rc=sock.recvAll(buffrd,1024,delay_us,20+8)).error())
            {
                if(icmp->id!=((uint16*)&buffrd[20])[2] || buffrd[20]!=ICMP_ECHOREPLY)
                {
                    rc=-1002;
                }
            }
        }
        rc=time::uStamp()-start;
    }

#if !defined(__linux) && !defined(__APPLE__)
    WSACleanup();
#endif
    return rc;
}

//////////////////////////////////////////////////////////////////////////
//Внутреннняя структура сервера
struct AServerInternal
{
    sockaddr_in  addr;
    SOCKET      msock;
    retCode    last_error;
    int initLevel;
    bool blocking;
};

server::server(connection::TransportType type, int port, bool blocking)
{
    internal = new AServerInternal;
    _type=type;
    _port=port;

    AServerInternal *hand=(AServerInternal*)internal;
    hand->initLevel=0;
    hand->last_error=0;
    hand->blocking=blocking;

    //инициализация библиотеки
#if !defined(__linux) && !defined(__APPLE__)
    WSADATA WsaData;
    int err = WSAStartup(MAKEWORD(1,1), &WsaData);

    if (err != 0)
    {
        hand->last_error=errFailInitLib();
        return;
    }
#endif
    hand->initLevel=1;        
}

server::~server()
{
    AServerInternal *hand=(AServerInternal*)internal;

    disable();
#if !defined(__linux) && !defined(__APPLE__)
    if(hand->initLevel>0)
    {
        WSACleanup();
    }
#endif
    delete hand;
}

retCode server::enable(int connLimit)
{
    AServerInternal *hand=(AServerInternal*)internal;
    if(!hand->initLevel)return hand->last_error;
    hand->last_error=0;

    //создание сёкета
    switch(_type)
    {
    case connection::UDP:
        hand->msock=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        break;
    case connection::TCP:
        hand->msock=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        break;
    default:
        hand->last_error=errInvalideTransportType();
        return hand->last_error;
    }

    if(hand->msock==INVALID_SOCKET)
    {
        hand->last_error=errSocketCreate();
        return hand->last_error;
    }
    hand->initLevel=2;

    if(hand->blocking)
    {
        unsigned long tmp=1;
        if(ioctlsocket(hand->msock,FIONBIO,&tmp)==SOCKET_ERROR)
        {
            disable();
            hand->last_error=errSocketUnblock();
            return hand->last_error;
        }
    }

    memset(&hand->addr,0,sizeof(hand->addr));
    hand->addr.sin_family=AF_INET;
    hand->addr.sin_port=htons(_port);
#if !defined(__linux) && !defined(__APPLE__)
    hand->addr.sin_addr.S_un.S_addr=htonl(INADDR_ANY);
#else
    hand->addr.sin_addr.s_addr=htonl(INADDR_ANY);
#endif
    if(bind(hand->msock,(sockaddr*)&hand->addr,sizeof(hand->addr))==SOCKET_ERROR)
    {        
        disable();
        hand->last_error=errSocketBind();
        return hand->last_error;
    }
    hand->initLevel=3;

    if(listen(hand->msock,(connLimit<0)?SOMAXCONN:connLimit)==SOCKET_ERROR)
    {
        disable();
        hand->last_error=errSocketListen();
        return hand->last_error;
    }

    hand->initLevel=4;
    return 0;
}

retCode server::disable()
{
    AServerInternal *hand=(AServerInternal*)internal;
    if(!hand->initLevel)return hand->last_error;

    if(hand->initLevel>1)
    {
        shutdown(hand->msock,SD_BOTH);
        closesocket(hand->msock);
        hand->initLevel=1;
        return 1;
    }
    return 0;
}

server& server::setOption(connection::OptionValue opt, const variant &val)
{
    options[opt]=val;
    return *this;
}

struct APeerInternal
{
    SOCKET          sock;
    sockaddr_in     addr;
    int port;
    retCode    last_error;
    int initLevel;
    connection::TransportType type;
    bool blocking;
};

peer* server::tryAccept()
{
    AServerInternal *hand=(AServerInternal*)internal;
    if(hand->initLevel!=4)return NULL;

    APeerInternal peerhand;

    hand->last_error=0;

#if defined(__linux) || defined(__APPLE__)
    socklen_t addrlen=sizeof(sockaddr_in);
#else
    int addrlen=sizeof(sockaddr_in);
#endif
    peerhand.sock=accept(hand->msock,(sockaddr*)&peerhand.addr,&addrlen);

    if(peerhand.sock==INVALID_SOCKET) return NULL;

    if(hand->blocking)
    {
        unsigned long tmp=1;
        if(ioctlsocket(peerhand.sock,FIONBIO,&tmp)==SOCKET_ERROR)
        {
            shutdown(peerhand.sock,SD_BOTH);
            closesocket(peerhand.sock);
            hand->last_error=errSocketCreate();
            return NULL;
        }
    }

    peerhand.blocking=hand->blocking;
    peerhand.initLevel=1;
    peerhand.last_error=0;
    peerhand.port=_port;
    peerhand.type=_type;

    for(int i=0;i<options.size();i++)
    {
        bool ok;
        int temp=options.value(i).toInt(&ok);
        if(!ok)continue;

        if(hand->initLevel>1)
        {
            if(options.key(i)==connection::ORecvBuffSize)
                setsockopt(peerhand.sock,SOL_SOCKET,SO_RCVBUF,(char*)&temp,sizeof(int));
            else if(options.key(i)==connection::OSendBuffSize)
                setsockopt(peerhand.sock,SOL_SOCKET,SO_SNDBUF,(char*)&temp,sizeof(int));
            else if(options.key(i)==connection::OKeepAlive)
                setsockopt(peerhand.sock,SOL_SOCKET,SO_KEEPALIVE,(char*)&temp,sizeof(int));
            else if(_type==connection::TCP && options.key(i)==connection::ONoDelay)
                setsockopt(peerhand.sock,IPPROTO_TCP,TCP_NODELAY,(char*)&temp,sizeof(int));
        }
    }

    return new peer(&peerhand);
}

retCode server::lastError()
{
    AServerInternal *hand=(AServerInternal*)internal;
    return hand->last_error;
}

//////////////////////////////////////////////////////////////////////
peer::peer(void *iDatum)
{
    APeerInternal *hand=new APeerInternal;
    *hand=*((APeerInternal*)iDatum);
    internal=hand;
}

peer::~peer()
{
    APeerInternal *hand=(APeerInternal*)internal;
    if(hand->initLevel)
    {
        shutdown(hand->sock,SD_BOTH);
        closesocket(hand->sock);
    }
}

retCode peer::send(const void *data, int size)
{
    APeerInternal *hand=(APeerInternal*)internal;
    retCode stat=status();
    if(stat.value())
    {
        int rval=::send(hand->sock,(char*)data,size,0);
        if(rval==SOCKET_ERROR)
        {
            rval=WSAGetLastError();
            if(!hand->blocking)
            {
                if(hand->type==connection::TCP && (rval==WSAENOBUFS || rval==WSAEWOULDBLOCK))return 0;
                if(hand->type==connection::UDP && rval==WSAEWOULDBLOCK)return 0;
            }
            else
            {
                if(hand->type==connection::TCP && rval==WSAENOBUFS)return 0;
            }
            shutdown(hand->sock,SD_BOTH);
            closesocket(hand->sock);
            hand->initLevel=0;
            hand->last_error=errFailOnWrite();
            return hand->last_error;
        }
        return rval;
    }
    if(!stat.error())return 0;
    return hand->last_error;
}

retCode peer::waitData(int time_ms)
{
    APeerInternal *hand=(APeerInternal*)internal;
    retCode stat=status();
    if(stat.value())
    {
        fd_set r;
        FD_ZERO(&r);
        FD_SET(hand->sock,&r);

        timeval delay;
        delay.tv_sec=time_ms/1000;
        delay.tv_usec=(time_ms%1000)*1000;

        if(select(0,&r,NULL,NULL,&delay)==SOCKET_ERROR)
        {
            shutdown(hand->sock,SD_BOTH);
            closesocket(hand->sock);
            hand->initLevel=0;
            hand->last_error=errFailOnRead();
            return hand->last_error;
        }
        if(FD_ISSET(hand->sock, &r))
        {
            return 1;
        }
    }
    if(!stat.error())return 0;
    return hand->last_error;
}

retCode peer::recv(void *data, int size)
{
    APeerInternal *hand=(APeerInternal*)internal;
    retCode stat=status();
    if(stat.value())
    {
        int rval=::recv(hand->sock,(char*)data,size,0);
        if(rval==SOCKET_ERROR)
        {
                rval=WSAGetLastError();
                if(!hand->blocking)
                {
                    if(hand->type==connection::TCP && (rval==WSAEWOULDBLOCK || rval==WSAEMSGSIZE))return 0;
                    if(hand->type==connection::UDP && rval==WSAEWOULDBLOCK)return 0;
                }
                else
                {
                    if(hand->type==connection::TCP && rval==WSAEMSGSIZE)return 0;
                }
                shutdown(hand->sock,SD_BOTH);
                closesocket(hand->sock);
                hand->initLevel=0;
                hand->last_error=errFailOnRead();
                return hand->last_error;
        }
        return rval;
    }
    if(!stat.error())return 0;
    return hand->last_error;
}

retCode peer::status()
{
    APeerInternal *hand=(APeerInternal*)internal;
    if(!hand->initLevel)return hand->last_error;
    return 1;
}

retCode peer::lastError()
{
    APeerInternal *hand=(APeerInternal*)internal;
    return hand->last_error;
}
