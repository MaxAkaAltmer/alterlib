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

#ifndef ANETWORK_H
#define ANETWORK_H

#include "astring.h"
#include "avariant.h"
#include "at_hash.h"

namespace alt {

    class connection
    {
    public:
        enum TransportType
        {
            TCP,
            UDP,
            ICMP
        };

        //////////////////////////////////////////////////////////////////////
        connection(TransportType type, const string &host, int port, bool blocking);
        ~connection();

        //////////////////////////////////////////////////////////////////////
        enum OptionValue
        {
            ORecvBuffSize,  // размер буфера приема
            OSendBuffSize,  // размер буфера отправки
            ONoDelay,       // запретить склеивание данных в крупные пакеты
            OKeepAlive      // ужерживать соединение
        };
        connection& setOption(OptionValue opt, const variant &val);

        //////////////////////////////////////////////////////////////////////
        retCode status();
        retCode lastError();

        //////////////////////////////////////////////////////////////////////
        retCode connect();
        retCode disconnect();

        //////////////////////////////////////////////////////////////////////
        retCode send(const void *data, int size); //может послать 0 байт - это нормально при асинхронном режиме
        retCode sendAll(const void *data, int size, int timeout_us);
        retCode recv(void *data, int size);
        retCode recvAll(void *data, int size, int timeout_us, int minimal_size=-1);
        retCode waitData(int time_ms);

        //////////////////////////////////////////////////////////////////////
        static bool isIP(const string &addr);
        static string toIP(const string &addr);
        static retCode ping(string addr, int delay_us);

        //////////////////////////////////////////////////////////////////////
        static string errorDescriptor(retCode code);

    private:

        TransportType _type;
        string _host;
        int _port;
        void *internal;
        alt::hash<OptionValue,variant> options;

    };

    class peer
    {
    public:
        peer(void *iDatum);
        ~peer();

        //////////////////////////////////////////////////////////////////////
        retCode send(const void *data, int size); //может послать 0 байт - это нормально при асинхронном режиме
        retCode recv(void *data, int size);
        retCode waitData(int time_ms);

        //////////////////////////////////////////////////////////////////////
        retCode status();
        retCode lastError();

    private:
        void *internal;
    };

    class server
    {
    public: //todo: предел подключений
        server(connection::TransportType type, int port, bool blocking);
        ~server();

        //////////////////////////////////////////////////////////////////////
        retCode enable(int connLimit=-1);
        retCode disable();

        //////////////////////////////////////////////////////////////////////
        server& setOption(connection::OptionValue opt, const variant &val);

        //////////////////////////////////////////////////////////////////////
        peer* tryAccept();

        //////////////////////////////////////////////////////////////////////
        retCode lastError();

    private:

        connection::TransportType _type;
        int _port;
        void *internal;
        alt::hash<connection::OptionValue,variant> options; //опции для подключаемых сёкетов

    };

} // namespace alt

#endif // ANETWORK_H
