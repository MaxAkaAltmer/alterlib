/*****************************************************************************

This is part of Alterlib - the free code collection under the MIT License
------------------------------------------------------------------------------
Copyright (C) 2006-2020 Maxim L. Grishin  (altmer@arts-union.ru)

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

#ifndef ASOCKET_H
#define ASOCKET_H

#include "astring.h"
#include "avariant.h"
#include "at_hash.h"

class ASocket
{
public:
    enum TransportType
    {
        TCP,
        UDP,
        ICMP
    };

    //////////////////////////////////////////////////////////////////////
    ASocket(TransportType type, const AString &host, int port, bool blocking);
    ~ASocket();

    //////////////////////////////////////////////////////////////////////
    enum OptionValue
    {
        ORecvBuffSize,  // размер буфера приема
        OSendBuffSize,  // размер буфера отправки
        ONoDelay,       // запретить склеивание данных в крупные пакеты
        OKeepAlive      // ужерживать соединение
    };
    ASocket& setOption(OptionValue opt, const AVariant &val);

    //////////////////////////////////////////////////////////////////////
    ARetCode status();
    ARetCode lastError();

    //////////////////////////////////////////////////////////////////////
    ARetCode connect();
    ARetCode disconnect();

    //////////////////////////////////////////////////////////////////////
    ARetCode send(const void *data, int size); //может послать 0 байт - это нормально при асинхронном режиме
    ARetCode sendAll(const void *data, int size, int timeout_us);
    ARetCode recv(void *data, int size);
    ARetCode recvAll(void *data, int size, int timeout_us, int minimal_size=-1);
    ARetCode waitData(int time_ms);

    //////////////////////////////////////////////////////////////////////
    static bool isIP(const AString &addr);
    static AString toIP(const AString &addr);
    static ARetCode ping(AString addr, int delay_us);

    //////////////////////////////////////////////////////////////////////
    static AString errorDescriptor(ARetCode code);

private:

    TransportType _type;
    AString _host;
    int _port;
    void *internal;
    ATHash<OptionValue,AVariant> options;

};

class APeer
{
public:
    APeer(void *iDatum);
    ~APeer();

    //////////////////////////////////////////////////////////////////////
    ARetCode send(const void *data, int size); //может послать 0 байт - это нормально при асинхронном режиме
    ARetCode recv(void *data, int size);
    ARetCode waitData(int time_ms);

    //////////////////////////////////////////////////////////////////////
    ARetCode status();
    ARetCode lastError();

private:
    void *internal;
};

class AServer
{
public: //todo: предел подключений
    AServer(ASocket::TransportType type, int port, bool blocking);
    ~AServer();

    //////////////////////////////////////////////////////////////////////
    ARetCode enable(int connLimit=-1);
    ARetCode disable();

    //////////////////////////////////////////////////////////////////////
    AServer& setOption(ASocket::OptionValue opt, const AVariant &val);

    //////////////////////////////////////////////////////////////////////
    APeer* tryAccept();

    //////////////////////////////////////////////////////////////////////
    ARetCode lastError();

private:

    ASocket::TransportType _type;
    int _port;
    void *internal;
    ATHash<ASocket::OptionValue,AVariant> options; //опции для подключаемых сёкетов

};

#endif // ASOCKET_H
