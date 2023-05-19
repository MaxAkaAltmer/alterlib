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

#ifndef BITOPCLASS_DEFINITION_HEADER
#define BITOPCLASS_DEFINITION_HEADER

#include "atypes.h"
#include "adelegate.h"

namespace alt {

    template <typename T>
    class bitSpaceBase
    {
    protected:
        uintz point =0;
        uint8 bitpoint = 0;
        uint8 bitset = 1;

        virtual uint8 memRead(uintz offset) = 0;
        virtual void memWrite(uintz offset, uint8 val) = 0;

        uintz convHoldOffset(uintz off, int hold)
        {
            if(!hold)return off;
            return 2*hold*(off/hold)+(off%hold);
        }

    public:
        bitSpaceBase(){}
        virtual ~bitSpaceBase(){}

        void setBitrate(int bits)
        {
            bitset=bits;
            if(bitset>sizeof(T)*8)bitset=sizeof(T)*8;
            if(bitset<1)bitset=1;
        }
        int bitrate()
        {
            return bitset;
        }
        void setPosition(uintz bytepos, uint8 bitpos)
        {
            point=bytepos;
            bitpoint=bitpos&7;
        }
        void setPosition(uintz bitoffset)
        {
            setPosition(bitoffset>>3,bitoffset&7);
        }

        uintz bytePosition(){return point;}
        uint8 bitPosition(){return bitpoint;}
        uintz size(){return point+((bitpoint)?1:0);}
        uintz position(){return (bytePosition()<<3)|((uintz)bitPosition());}

        T read()
        {
         const static uint8 mas[]={1,3,7,15,31,63,127,255};
         T retval=0;
         int8 bitcnt=bitset;

            if((8-bitpoint)>bitset)
            {
                retval=memRead(point);
                retval>>=(bitpoint);
                retval&=mas[bitset-1];
                bitpoint+=bitset;
                return retval;
            }
            if(bitpoint)
            {
                retval=memRead(point)>>(bitpoint);
                point++;
                bitcnt-=8-bitpoint;
            }
            while(bitcnt>=8)
            {
                if((bitset-bitcnt))
                    retval|=((T)memRead(point))<<(bitset-bitcnt);
                else
                    retval|=memRead(point);
                point++;
                bitcnt-=8;
            }
            if(bitcnt)
            {
                if((bitset-bitcnt))
                    retval|=((T)(memRead(point)&mas[bitcnt-1]))<<(bitset-bitcnt);
                else
                    retval|=(memRead(point)&mas[bitcnt-1]);
                bitpoint=bitcnt;
            }
            else
            {
                bitpoint=0;
            }
            return retval;
        }

        T readBig()
        {
         const static uint8 mas[]={0,1,3,7,15,31,63,127,255};
         T retval=0;
         int8 bitcnt=bitset;

            if((8-bitpoint)>bitset)
            {
                retval=memRead(point);
                retval>>=(8-bitpoint-bitset);
                retval&=mas[bitset];
                bitpoint+=bitset;
                return retval;
            }
            if(bitpoint)
            {
                retval=memRead(point)&mas[8-bitpoint];
                point++;
                bitcnt-=8-bitpoint;
            }
            while(bitcnt>=8)
            {
                retval<<=8;
                retval|=memRead(point);
                point++;
                bitcnt-=8;
            }
            if(bitcnt)
            {
                retval<<=bitcnt;
                retval|=memRead(point)>>(8-bitcnt);

            }
            bitpoint=bitcnt;

            return retval;
        }

        T readHold(int hold)
        {
         const static uint8 mas[]={1,3,7,15,31,63,127,255};
         T retval=0;
         int8 bitcnt=bitset;

            if((8-bitpoint)>bitset)
            {
                retval=memRead(convHoldOffset(point,hold));
                retval>>=(bitpoint);
                retval&=mas[bitset-1];
                bitpoint+=bitset;
                return retval;
            }
            if(bitpoint)
            {
                retval=memRead(convHoldOffset(point,hold))>>(bitpoint);
                point++;
                bitcnt-=8-bitpoint;
            }
            while(bitcnt>=8)
            {
                if((bitset-bitcnt))
                    retval|=((T)memRead(convHoldOffset(point,hold)))<<(bitset-bitcnt);
                else
                    retval|=memRead(convHoldOffset(point,hold));
                point++;
                bitcnt-=8;
            }
            if(bitcnt)
            {
                if((bitset-bitcnt))
                    retval|=((T)(memRead(convHoldOffset(point,hold))&mas[bitcnt-1]))<<(bitset-bitcnt);
                else
                    retval|=(memRead(convHoldOffset(point,hold))&mas[bitcnt-1]);
                bitpoint=bitcnt;
            }
            else
            {
                bitpoint=0;
            }
            return retval;
        }

        T readHoldBig(int hold)
        {
         const static uint8 mas[]={0,1,3,7,15,31,63,127,255};
         T retval=0;
         int32 bitcnt=bitset;

            if((8-bitpoint)>bitset)
            {
                retval=memRead(convHoldOffset(point,hold));
                retval>>=8-bitpoint-bitset;
                retval&=mas[bitset];
                bitpoint+=bitset;
                return retval;
            }
            if(bitpoint)
            {
                retval=memRead(convHoldOffset(point,hold))&mas[8-bitpoint];
                point++;
                bitcnt-=8-bitpoint;
            }
            while(bitcnt>=8)
            {
                retval<<=8;
                retval|=memRead(convHoldOffset(point,hold));
                point++;
                bitcnt-=8;
            }
            if(bitcnt)
            {
                retval<<=bitcnt;
                retval|=memRead(convHoldOffset(point,hold))>>(8-bitcnt);
            }
            bitpoint=bitcnt;

            return retval;
        }

        void write(T data)
        {
         const static uint8 mas[]={1,3,7,15,31,63,127,255};
         int8 bitcnt=bitset;
         uint8 temp,mask;

            if((8-bitpoint)>bitset)
            {
                temp=memRead(point);
                        mask=(mas[bitset-1]<<(bitpoint));
                temp&=~(mask);
                temp|=(data<<(bitpoint))&mask;
                memWrite(point,temp);
                bitpoint+=bitset;
                return;
            }
            if(bitpoint)
            {
                temp=memRead(point);
                        mask=mas[7-bitpoint];
                temp&=~(mask<<bitpoint);

                temp|=(data&mask)<<bitpoint;

                memWrite(point,temp);
                point++;
                bitcnt-=8-bitpoint;
            }
            while(bitcnt>=8)
            {
                if(bitset-bitcnt)temp=(data>>(bitset-bitcnt))&0xff;
                else temp=data&0xff;
                memWrite(point,temp);
                point++;
                bitcnt-=8;
            }
            if(bitcnt)
            {
                mask=(mas[bitcnt-1]);
                temp=memRead(point)&(~mask);
                temp|=(data>>(bitset-bitcnt))&mask;
                memWrite(point,temp);
                bitpoint=bitcnt;
            }
            else
            {
                bitpoint=0;
            }

        }

        void writeBig(T data)
        {
         const static uint8 mas[]={0,1,3,7,15,31,63,127,255};
         int8 bitcnt=bitset;
         uint8 temp,mask;

            if((8-bitpoint)>bitset)
            {
                temp=memRead(point);
                mask=(mas[bitset]<<(8-bitpoint-bitset));
                temp&=~(mask);
                temp|=(data<<(8-bitpoint-bitset))&mask;
                memWrite(point,temp);
                bitpoint+=bitset;
                return;
            }
            if(bitpoint)
            {
                temp=memRead(point);
                mask=mas[8-bitpoint];
                temp&=~(mask);

                temp|=(data&mask);

                memWrite(point,temp);
                point++;
                bitcnt-=8-bitpoint;
            }
            while(bitcnt>=8)
            {
                if(bitcnt)temp=(data>>(bitcnt))&0xff;
                else temp=data&0xff;
                memWrite(point,temp);
                point++;
                bitcnt-=8;
            }
            if(bitcnt)
            {
                mask=(mas[bitcnt]) << (8-bitcnt);
                temp=memRead(point)&(~mask);
                temp|=(data<<(8-bitcnt))&mask;
                memWrite(point,temp);
            }
            bitpoint=bitcnt;

        }


        T readBig(uint8 bits)
        {
            setBitrate(bits);
            return readBig();
        }

        T read(uint8 bits)
        {
            setBitrate(bits);
            return read();
        }

        void write(T data, uint8 bits)
        {
            setBitrate(bits);
            write(data);
        }

        void writeBig(T data, uint8 bits)
        {
            setBitrate(bits);
            writeBig(data);
        }

        void skip(uintz bits)
        {
            bits+=bitpoint;
            point+=(bits>>3);
            bitpoint=bits&7;
        }

        void clearEnd()
        {
         const static uint8 mas[]={1,3,7,15,31,63,127,255};
         uint8 temp;
                if(bitpoint)
                {
                        temp=memRead(point);
                        temp&=mas[bitpoint-1];
                        memWrite(point,temp);
                }
        }

        void removeBytes()
        {
            if(bitpoint)
                memWrite(0,memRead(point));
            point=0;
        }

    };

    template <typename T, typename A>
    class bitSpaceExecutor: public bitSpaceBase<T>
    {
    protected:
        A buffer;

        alt::delegate<uint8,A> _mem_read8;
        alt::delegateProc<A,uint8> _mem_write8;

        uint8 memRead(uintz offset) final
        {
            if(!buffer)return 0;
            return _mem_read8(offset+buffer);
        }
        void memWrite(uintz offset, uint8 val) final
        {
            if(!buffer)return;
            _mem_write8(offset,val);
        }

    public:
        bitSpaceExecutor(alt::delegate<uint8,A> reader)
            : bitSpaceBase<T>()
        {
            buffer=0;
            _mem_read8=reader;
        }
        bitSpaceExecutor(alt::delegate<uint8,A> reader, alt::delegateProc<A,uint8> writer)
            : bitSpaceBase<T>()
        {
            buffer=0;
            _mem_read8=reader;
            _mem_write8 = writer;
        }
        virtual ~bitSpaceExecutor() {}

        void attachBuffer(A buff)
        {
            buffer=buff;
            bitSpaceBase<T>::point=0;
            bitSpaceBase<T>::bitpoint=0;
        }

    };


    template <typename T>
    class bitSpace: public bitSpaceBase<T>
    {
    protected:
        uint8 *buffer = nullptr;
        uintz limit = 0;
        uintz alloc = 0;

        uint8 memRead(uintz offset) final
        {
            if(!buffer)
                return 0;

            if(limit>0)
            {
                  if(limit<=offset)
                      return 0;
            }
            if(alloc>0)
            {
                  if(alloc<=offset)
                      return 0;
            }
            return buffer[offset];
        }

        void memWrite(uintz offset, uint8 val) final
        {
            if(!bufferIsExternal() && alloc<=offset)
            {
                uintz count = utils::upsize((offset+1));
                uint8 *tmp=new uint8[count];
                if(alloc)
                {
                    utils::memcpy(tmp,buffer,alloc);
                    delete []buffer;
                }
                buffer=tmp;
                alloc=count;
            }
            if(!buffer) return;
            if(limit>0)
            {
                  if(limit<=offset)
                      return;
            }
            buffer[offset] = val;
        }

    public:
        bitSpace()
            : bitSpaceBase<T>()
        {
        }
        bitSpace(void *buff, uintz size = 0)
            : bitSpaceBase<T>()
        {
            buffer=(uint8*)buff;
            limit = size;
        }
        bitSpace(uintz size)
            : bitSpaceBase<T>()
        {
            alloc = size;
            buffer=new uint8[alloc];
        }
        virtual ~bitSpace()
        {
            if(alloc)
                delete []buffer;
        }

        bool bufferIsExternal()
        {
            return (buffer && !alloc);
        }

        void resetBuffer(void *buff, uintz size = 0)
        {
            if(alloc)
            {
                delete []buffer;
                alloc = 0;
                buffer = nullptr;
            }
            buffer=(uint8*)buff;
            limit = size;
            bitSpaceBase<T>::point=0;
            bitSpaceBase<T>::bitpoint=0;

            if(!buffer && size)
            {
                alloc = size;
                buffer=new uint8[size];
            }
        }

        void reAllocate(uintz bits)
        {
            if(bufferIsExternal())
                return;
            if(alloc!=((bits+7)>>3))
            {
                uint8 *tmp=new uint8[((bits+7)>>3)];
                if(alloc)
                {
                    utils::memcpy(tmp,buffer,alloc);
                    delete []buffer;
                }
                buffer=tmp;
                alloc=((bits+7)>>3);
            }
        }

        uint8* operator()(){return buffer;}

        void swapLast(uint32 cnt)
        {
         bitSpace sp1,sp2;
         uint32 base1=bitSpaceBase<T>::position()-bitSpaceBase<T>::bitset*cnt;
         uint32 base2=bitSpaceBase<T>::position()-bitSpaceBase<T>::bitset;
         uint32 i,n;
         uint32 tmp1,tmp2;

                sp1.resetBuffer(buffer);
                sp2.resetBuffer(buffer);
                sp1.setBitrate(bitSpaceBase<T>::bitset);
                sp2.setBitrate(bitSpaceBase<T>::bitset);

                n=cnt>>1;
                for(i=0;i<n;i++)
                {
                        sp1.setPosition(base1>>3, base1&7);
                        tmp1=sp1.read();
                        sp2.setPosition(base2>>3, base2&7);
                        tmp2=sp2.read();
                        sp1.setPosition(base1>>3, base1&7);
                        sp1.write(tmp2);
                        sp2.setPosition(base2>>3, base2&7);
                        sp2.write(tmp1);
                        base1+=bitSpaceBase<T>::bitset;
                        base2-=bitSpaceBase<T>::bitset;
                }
        }

    };

} // namespace alt

#endif
