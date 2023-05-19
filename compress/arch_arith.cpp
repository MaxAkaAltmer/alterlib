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

#include "arch_arith.h"

ArithEncoder::ArithEncoder()
{

}
ArithEncoder::~ArithEncoder()
{

}

void ArithEncoder::Start(alt::bitSpace<uint32> &ardata, uint32 total_size)
{
        low = 0;
        high = 0xffff;
        underflow_bits = 0;
        model.Reset();

        _pref_pqs_encoder(model.getModel(),ardata,1,3,0);
        if(total_size>0)_pref_pqs_encoder(total_size-1,ardata,1,7,0);
}

uint32 ArithEncoder::Put(const uint8 *data, uint32 size, alt::bitSpace<uint32> &ardata)
{
 uint32 i;

    ardata.setBitrate(1);

    for(i=0;i<size;i++)
    {        
        int32 range = (int32) (high - low) + 1;
        high = low + (uint16) (( range * model.Hight(data[i]) ) / (model.Scale()) - 1 );
        low = low + (uint16) (( range * model.Low(data[i]) ) / (model.Scale()) );

        for ( ; ; )
        {
            if ( ( high & 0x8000 ) == ( low & 0x8000 ) )
            {
                ardata.write((high>>(15))&1);
                while ( underflow_bits > 0 )
                {
                    ardata.write(((~high)>>(15))&1);
                    underflow_bits--;
                }
            }
            else if ( ( low & 0x4000 ) && !( high & 0x4000 ))
            {
                underflow_bits += 1;
                low &= 0x3fff;
                high |= 0x4000;
            }
            else
            {
                break;
            }

            low <<= 1;
            high <<= 1;
            high |= 1;
        }
        model.Update(data[i]);

    }
    return i;
}

void ArithEncoder::Stop(alt::bitSpace<uint32> &ardata)
{
        ardata.setBitrate(1);

        ardata.write( ((low & 0x4000)>>(14)) );
        underflow_bits++;
        while ( underflow_bits-- > 0 )
                ardata.write( (((~low) & 0x4000)>>(14)) );

        ardata.clearEnd();
}

//------------------------------------------------------------------------------

ArithDecoder::ArithDecoder()
{

}
ArithDecoder::~ArithDecoder()
{

}

uint32 ArithDecoder::Start(alt::bitSpace<uint32> &ardata)
{
 uint32 i;

    if(!model.Reset(_pref_pqs_decoder(ardata,1,3,0)))
    {
        ending=true;
        return 0;
    }

    total_size=_pref_pqs_decoder(ardata,1,7,0)+1;

    ardata.setBitrate(1);
    code = 0;
    for ( i = 0 ; i < 16 ; i++ )
    {
        code <<= 1;
        code += (uint16)ardata.read();
    }
    low = 0;
    high = 0xffff;
    underflow_bits=0;
    bcnt=0;

    ending=false;

    return total_size;
}

int ArithDecoder::Get(uint8 *buff, uint32 size, alt::bitSpace<uint32> &ardata)
{
    uint32 j,i;
    int32 range;
    int16 count;

    if(ending)
    {
        return 0;
    }

    for(i=0;i<size;i++)
    {
        range = (int32) (high - low) + 1;
        count = ((( (int32) (code - low) + 1 ) * (model.Scale()) -1 ) / range );

        j=model.Index(count);

        buff[i]=j;
        total_size--;
        if(!total_size)
        {
            ending=true;
            i++;
            break;
        }

        high = low + (uint16) (( range * model.Hight(j) ) / (model.Scale()) - 1 );
        low = low + (uint16) (( range * model.Low(j) ) / (model.Scale()) );

        model.Update(j);

        for ( ; ; )
        {
            if ( ( high & 0x8000 ) == ( low & 0x8000 ) )
            {
                bcnt+=underflow_bits+1;
                underflow_bits=0;
            }
            else if ((low & 0x4000) && !(high & 0x4000) )
            {
                underflow_bits++;
                code ^= 0x4000;
                low   &= 0x3fff;
                high  |= 0x4000;
            }
            else
            {
                break;
            }

            low <<= 1;
            high <<= 1;
            high |= 1;
            code <<= 1;

            code += ardata.read()&1;
        }

    }
    return i;
}

uint32 ArithDecoder::Stop()
{    
    bcnt+=underflow_bits+1;

    return bcnt;
}

