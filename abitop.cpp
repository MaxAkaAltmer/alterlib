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

#include "abitop.h"
#include <memory.h>

uint32 ABitReaderBigHight_A32::Read()
{
 const static uint8 mas[]={0,1,3,7,15,31,63,127,255};
 uint32 retval=0;
 int32 bitcnt=bitset;

 	if(!buf)return retval;
 	if((8-bitpoint)>bitset)
 	{
        retval=_mem_read8(buf+point);
 		retval>>=8-bitpoint-bitset;
 		retval&=mas[bitset];
 		bitpoint+=bitset;
 		return retval;
 	}
 	if(bitpoint)
 	{
        retval=_mem_read8(buf+point)&mas[8-bitpoint];
		point++;
 		bitcnt-=8-bitpoint;
 	}
 	while(bitcnt>=8)
 	{
		retval<<=8;
        retval|=_mem_read8(buf+point);
        point++;
 		bitcnt-=8;
 	}
 	if(bitcnt)
 	{
		retval<<=bitcnt;
        retval|=_mem_read8(buf+point)>>(8-bitcnt);
 	}
 	bitpoint=bitcnt;

 	return retval; 	 	
}


uint32 ABitReaderBigHight_A32::ReadHold(int hold)
{
 const static uint8 mas[]={0,1,3,7,15,31,63,127,255};
 uint32 retval=0;
 int32 bitcnt=bitset;

    if(!buf)return retval;
    if((8-bitpoint)>bitset)
    {
        retval=_mem_read8(buf+convOffset(point,hold));
        retval>>=8-bitpoint-bitset;
        retval&=mas[bitset];
        bitpoint+=bitset;
        return retval;
    }
    if(bitpoint)
    {
        retval=_mem_read8(buf+convOffset(point,hold))&mas[8-bitpoint];
        point++;
        bitcnt-=8-bitpoint;
    }
    while(bitcnt>=8)
    {
        retval<<=8;
        retval|=_mem_read8(buf+convOffset(point,hold));
        point++;
        bitcnt-=8;
    }
    if(bitcnt)
    {
        retval<<=bitcnt;
        retval|=_mem_read8(buf+convOffset(point,hold))>>(8-bitcnt);
    }
    bitpoint=bitcnt;

    return retval;
}

uint32 ABitReaderBigHight_A32::Read(uint8 bits)
{
	SetBitRate(bits);
	return Read();
}

uint32 ABitSpace::Read()
{
 const static uint8 mas[]={1,3,7,15,31,63,127,255};
 uint32 retval=0;
 int8 bitcnt=bitset;
    if(!buf)return 0;

    if(read_limit>0)
    {
          if((uint32)read_limit<(((bitpoint+bitcnt+7)>>3)+point))return 0;
    }

    if((8-bitpoint)>bitset)
    {
        retval=buf[point];
        retval>>=(bitpoint);
        retval&=mas[bitset-1];
        bitpoint+=bitset;
        return retval;
    }
    if(bitpoint)
    {
        retval=buf[point++]>>(bitpoint);
        bitcnt-=8-bitpoint;
    }
    while(bitcnt>=8)
    {
        if((bitset-bitcnt))retval|=((uint32)buf[point++])<<(bitset-bitcnt);
                else retval|=buf[point++];
        bitcnt-=8;
    }
    if(bitcnt)
    {
        if((bitset-bitcnt))retval|=((uint32)(buf[point]&mas[bitcnt-1]))<<(bitset-bitcnt);
                else retval|=(buf[point]&mas[bitcnt-1]);
        bitpoint=bitcnt;
    }
    else
    {
        bitpoint=0;
    }
    return retval;
}

uint32 ABitSpace::Read(uint8 bits)
{
    SetBitRate(bits);
    return Read();
}


void ABitSpace::Write(uint32 data)
{
 const static uint8 mas[]={1,3,7,15,31,63,127,255};
 int8 bitcnt=bitset;
 uint8 temp,mask;
    if(!buf)return; //selfmodefication here! :)
    if((8-bitpoint)>bitset)
    {
        temp=buf[point];
                mask=(mas[bitset-1]<<(bitpoint));
        temp&=~(mask);
        temp|=(data<<(bitpoint))&mask;
        buf[point]=temp;
        bitpoint+=bitset;
        return;
    }
    if(bitpoint)
    {
        temp=buf[point];
                mask=mas[7-bitpoint];
        temp&=~(mask<<bitpoint);

        temp|=(data&mask)<<bitpoint;

        buf[point++]=temp;
        bitcnt-=8-bitpoint;
    }
    while(bitcnt>=8)
    {
        if(bitset-bitcnt)temp=(data>>(bitset-bitcnt))&0xff;
        else temp=data&0xff;
        buf[point++]=temp;
        bitcnt-=8;
    }
    if(bitcnt)
    {
        mask=(mas[bitcnt-1]);
        temp=buf[point]&(~mask);
        temp|=(data>>(bitset-bitcnt))&mask;
        buf[point]=temp;
        bitpoint=bitcnt;
    }
    else
    {
        bitpoint=0;
    }

}

void ABitSpace::Write(uint32 data, uint8 bits)
{
    SetBitRate(bits);
    Write(data);
}

void ABitSpace::ClearEnd()
{
 const static uint8 mas[]={1,3,7,15,31,63,127,255};
 uint8 temp;
        if(bitpoint)
        {
                temp=buf[point];
                temp&=mas[bitpoint-1];
                buf[point]=temp;
        }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

AAutoBitSpace::~AAutoBitSpace()
{
        if(alloc)delete []buf;
}

void AAutoBitSpace::ReAllocate(uint32 bits)
{
        if(alloc!=((bits+7)>>3))
        {
            uint8 *tmp=new uint8[((bits+7)>>3)];
            if(alloc)
            {
                memcpy(tmp,buf,alloc);
                delete []buf;
            }
            buf=tmp;
            alloc=((bits+7)>>3);
        }
}
void AAutoBitSpace::SetPos(uint32 bitpos)
{
        if(alloc<((bitpos+7)>>3))
        {
            uint8 *tmp=new uint8[((bitpos+7)>>3)];
            if(alloc)
            {
                memcpy(tmp,buf,alloc);
                delete []buf;
            }
            buf=tmp;
            alloc=((bitpos+7)>>3);
        }
        SetPosition(bitpos>>3, bitpos&7);
}

void AAutoBitSpace::Write(uint32 data)
{
        if((point+((bitpoint+bitset+7)>>3))>=alloc)
        {
            uint8 *tmp=new uint8[point+((bitpoint+bitset+7)>>3)+32];
            if(alloc)
            {
                memcpy(tmp,buf,alloc);
                delete []buf;
            }
            buf=tmp;
            alloc=(point+((bitpoint+bitset+7)>>3))+32;
        }
        ABitSpace::Write(data);
}
void AAutoBitSpace::Write(uint32 data, uint8 bits)
{
        SetBitRate(bits);
        Write(data);
}

void AAutoBitSpace::removeBytes()
{
    if(bitpoint)buf[0]=buf[point];
    point=0;
}

void AAutoBitSpace::SwapLast(uint32 cnt)
{
 ABitSpace sp1,sp2;
 uint32 base1=GetPos()-bitset*cnt;
 uint32 base2=GetPos()-bitset;
 uint32 i,n;
 uint32 tmp1,tmp2;

        sp1.AttachBuffer(buf);
        sp2.AttachBuffer(buf);
        sp1.SetBitRate(bitset);
        sp2.SetBitRate(bitset);

        n=cnt>>1;
        for(i=0;i<n;i++)
        {
                sp1.SetPosition(base1>>3, base1&7);
                tmp1=sp1.Read();
                sp2.SetPosition(base2>>3, base2&7);
                tmp2=sp2.Read();
                sp1.SetPosition(base1>>3, base1&7);
                sp1.Write(tmp2);
                sp2.SetPosition(base2>>3, base2&7);
                sp2.Write(tmp1);
                base1+=bitset;
                base2-=bitset;
        }

}


