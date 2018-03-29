/*****************************************************************************

This is part of Alterlib - the free code collection under the MIT License
------------------------------------------------------------------------------
Copyright (C) 2006-2018 Maxim L. Grishin  (altmer@arts-union.ru)

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

#include "types.h"

//todo: тотальный рефакторинг

class ABitReaderBigHight_A32
{
protected:
	uint32 buf;
	uint32 point;
	int32 bitpoint;
    int32 bitset;

    ADelegate<uint8,uint32> _mem_read8;

    uint32 convOffset(uint32 off, int hold)
    {
        if(!hold)return off;
        return 2*hold*(off/hold)+(off%hold);
    }

public:
    ABitReaderBigHight_A32(ADelegate<uint8,uint32> funct)
    {
        buf=0;
        bitset=1;
        point=0;
        bitpoint=0;
        _mem_read8=funct;
    }
    void AttachBuffer(uint32 buff)
	{
        buf=buff;
		point=0;
		bitpoint=0;
    }
	void SetBitRate(uint8 bits)
	{
		bitset=bits;
        if(bitset>32)bitset=32;
        if(!bitset)bitset=1;
    }

	void SetPosition(uint32 bytepos, uint8 bitpos)
	{
		point=bytepos;
		bitpoint=bitpos;
    }
	
    void SetPos(uint32 bitpos){SetPosition(bitpos>>3,bitpos&7);}

    uint32 GetBytePose(){return point;}

    uint32 GetDataSize(){return point+(bitpoint?1:0);}
    
    uint32 Read();
    uint32 ReadHold(int hold);
    uint32 Read(uint8 bits);

	void Skip(uint32 bits)
	{
		bits+=bitpoint;
		point+=(bits>>3);
		bitpoint=bits&7;
    }
	
};


class ABitSpace
{
protected:
    uint8 *buf;
    uint32 point;
    uint8 bitpoint;
    uint8 bitset;
    int read_limit;
public:
    ABitSpace()
    {
        buf=NULL;
        bitset=1;
        point=0;
        bitpoint=0;
        read_limit=-1;
    }
    ABitSpace(void *buff)
    {
        buf=(uint8*)buff;
        point=0;
        bitpoint=0;
        bitset=1;
        read_limit=-1;
    }
    void SetReadLimit(int val)
    {
        read_limit=val;
    }
    void AttachBuffer(void *buff)
    {
        buf=(uint8*)buff;
        point=0;
        bitpoint=0;
    }
    void SetBitRate(uint8 bits)
    {
        bitset=bits;
        if(bitset>64)bitset=64;
        if(!bitset)bitset=1;
    }
    uint32 GetBitrate()
    {
        return bitset;
    }
    void SetPosition(uint32 bytepos, uint8 bitpos)
    {
        point=bytepos;
        bitpoint=bitpos;
    }

    uint32 GetBytePos(){return point;}
    uint8 GetBitPos(){return bitpoint;}
    uint32 GetBytesUsed(){return point+((bitpoint)?1:0);}
    uint32 GetPos(){return (GetBytePos()<<3)|((uint32)GetBitPos());}

    void SetPos(uint32 bitpos){SetPosition(bitpos>>3,bitpos&7);}

    uint32 Read();
    uint32 Read(uint8 bits);

    void Write(uint32 data);
    void Write(uint32 data, uint8 bits);

    void ClearEnd();

};

class AAutoBitSpace: private ABitSpace
{
private:
    uint32 alloc;   //объем выделенной памяти
public:

    uint32 Read(){return ABitSpace::Read();}
    uint32 Read(uint8 bits){return ABitSpace::Read(bits);}

    void ClearEnd(){ABitSpace::ClearEnd();}
    void SetBitRate(uint8 bits){ABitSpace::SetBitRate(bits);}

    uint32 GetBytePos(){return ABitSpace::GetBytePos();}
    uint32 GetPos(){return ABitSpace::GetPos();}
    uint32 GetBytesUsed(){return ABitSpace::GetBytesUsed();}

    AAutoBitSpace()
    {
        alloc=0;
    }

    ~AAutoBitSpace();

    void ReAllocate(uint32 bits);
    void SetPos(uint32 bitpos);

    void Write(uint32 data);
    void Write(uint32 data, uint8 bits);

    void SwapLast(uint32 cnt);

    uint8* GetBuff(){return buf;}

    void removeBytes();

};


#endif
