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

#ifndef AT_RING_H
#define AT_RING_H

namespace alt {

    template <class T>
    class ring
    {
     private:
            T *buff;
            volatile int len,up,down;
     public:
            ring(){buff=0;len=1;up=down=0;}
            ring(int size){buff=new T[size+1];len=size+1;up=down=0;}
            ~ring(){if(buff)delete []buff;}

            void Resize(int size)
            {
                if(buff)delete []buff;
                buff=new T[size+1];len=size+1;up=down=0;
            }

            int Read(T *data, int size)
            {
                int cnt=Size();
                if(cnt>size)cnt=size;
                for(int i=0;i<cnt;i++)
                {
                    data[i]=Get();
                }
                return cnt;
            }

            bool WriteBlock(const T *block, int size)
            {
                if(Allow()<size)return false;
                for(int i=0;i<size;i++)
                {
                    buff[up]=block[i];
                    up=(up+1)%len;
                }
                return true;
            }

            int Limit(){return len-1;}

            void forcedPush(const T &val) //не для многопоточного применения!
            {
                if(!Allow())Get();
                buff[up]=val;
                up=(up+1)%len;
            }

            bool Push(const T &val)
            {
                if(!Allow())return false;
                buff[up]=val;
                up=(up+1)%len;
                return true;
            }

            int Size() const
            {
                volatile int tup=up,tdown=down;
                if(tup>=tdown)return tup-tdown;
                return len-tdown+tup;
            }

            //размер линейного блока для вычитывания
            int blockSizeToRead()
            {
                volatile int tup=up,tdown=down;
                if(tup>=tdown)return tup-tdown;
                return len-tdown;
            }

            //размер линейного блока для записи
            int blockSizeToWrite()
            {
                volatile int tup=up,tdown=down;
                if(tup>=tdown)return len-tup-(tdown?0:1);
                return tdown-tup-1;
            }

            //указатель на начало данных
            T* startPoint()
            {
                return &buff[down];
            }

            //указатель на место записи
            T* afterPoint()
            {
                return &buff[up];
            }

            T Get()
            {
             T rv;
                rv=buff[down];
                if(!Size())return rv;
                down=(down+1)%len;
                return rv;
            }

            int Allow()
            {
                if(!buff)return 0;
                volatile int tup=up,tdown=down;
                if(tup<tdown)return tdown-tup-1;
                return len-tup+tdown-1;
            }
            T& operator[](int ind)
            {
                if(ind<0)
                {
                    ind = Size() + ind;
                }
                return buff[(down+ind)%len];
            }
            void Free(int size=-1)
            {
                if(size<0)down=up;
                else down=(down+size)%len;
            }
            void Added(int size)
            {
                up=(up+size)%len;
            }
    };

} // namespace alt

#endif // AT_RING_H
