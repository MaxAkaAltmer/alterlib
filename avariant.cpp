﻿/*****************************************************************************

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

#include "avariant.h"
#include <math.h>

using namespace alt;

bool variant::toBool(bool *ok) const
{
    if(ok)*ok=true;
    switch(type)
    {
    case tBool:
        return data.vBool;
    case tInt:
        return data.vInt;
    case tReal:
        return imath::round<intx>(data.vReal);
    case tString: {
        string tmp = (*data.vString).toLower();
        if(tmp=="true" || tmp=="ok" || tmp=="yes")
            return true;
        return false; }
    case tFraction:
        return imath::round<intx>(data.vFraction->to<realx>());
    default:
        if(ok)*ok=false;
        break;
    };
    return false;
}

intx variant::toInt(bool *ok) const
{
    if(ok)*ok=true;
    switch(type)
    {
    case tBool:
        return data.vBool?1:0;
    case tInt:
        return data.vInt;
    case tReal:
        return imath::round<intx>(data.vReal);
    case tString:
        return (*data.vString).toInt<intx>(10,ok);
    case tPointer:
        return ptr2int(data.vPointer);
    case tFraction:
        return data.vFraction->to<intx>();
    default:
        if(ok)*ok=false;
        break;
    }    
    return 0;
}

fraction<intx> variant::toFraction(bool *ok) const
{
    if(ok)*ok=true;
    switch(type)
    {
    case tBool:
        return data.vBool?1:0;
    case tInt:
        return data.vInt;
    case tReal:
        return fraction<intx>::from(data.vReal);
    case tString: //todo:: parsing
        return fraction<intx>::from((*data.vString).toReal<realx>());
    case tFraction:
        return *data.vFraction;
    default:
        if(ok)*ok=false;
        break;
    }
    return 0;
}

realx variant::toReal(bool *ok) const
{
    if(ok)*ok=true;
    switch(type)
    {
    case tBool:
        return data.vBool?1:0;
    case tInt:
        return data.vInt;
    case tReal:
        return data.vReal;
    case tString:
        return (*data.vString).toReal<realx>();
    case tFraction:
        return data.vFraction->to<realx>();
    default:
        if(ok)*ok=false;
        break;
    }    
    return 0.0;
}

string variant::toString(bool *ok, char sep) const
{
    if(ok)*ok=true;
    switch(type)
    {
    case tBool:
        if(data.vBool)return "true";
        return "false";
    case tInt:
        return string::fromInt(data.vInt);
    case tString:
        return (*data.vString);
    case tReal:
        return string::fromReal(data.vReal,20);
    case tData:
        return (*data.vData).toHex();
    case tFraction:
        return data.vFraction->toString();
    case tDim: {
        string sep_str = string::fromFix(&sep,1);
        return data.vDim->toString(sep_str); }
    case tArray: {
        string rv;
        for(int i=0;i<(*data.vArray).size();i++)
        {
            if(i) rv.append(sep);
            rv += (*data.vArray)[i].toString();
        }
        return rv; }
    default:
        if(ok)*ok=false;
        break;
    }    
    return string();
}

uintx variant::size() const
{
    switch(type)
    {
    case tString:
        return data.vString->size();
    case tData:
        return data.vData->size();
    case tArray:
        return data.vArray->size();
    case tInvalide:
        return 0;
    case tDim:
        return data.vDim->size();
    case tHash:
        return data.vHash->size();
    default:
        break;
    }
    return 0;
}

byteArray variant::toData(bool *ok) const
{
    if(isString())
    {
        if(ok)*ok=true;
        return byteArray::fromHex((*data.vString)());
    }
    if(isData())
    {
        if(ok)*ok=true;
        return *data.vData;
    }
    if(ok)*ok=false;
    return byteArray();
}

hash<string,variant> variant::toHash(bool *ok) const
{
    if(isHash())
    {
        if(ok)*ok=true;
        return (*data.vHash);
    }
    if(ok)*ok=false;
    return hash<string,variant>();
}

dimensions<uintz> variant::toDim(bool *ok) const
{
    if(isDim())
    {
        if(ok)*ok=true;
        return (*data.vDim);
    }
    if(ok)*ok=false;
    return dimensions<uintz>();
}

dimensions<uintz>* variant::pointDim()
{
    if(isDim())return data.vDim;
    return nullptr;
}

array<variant> variant::toArray(bool *ok) const
{
    if(isArray())
    {
        if(ok)*ok=true;
        return (*data.vArray);
    }
    if(ok)*ok=false;
    return array<variant>();
}

array<variant>* variant::pointArray()
{
    if(isArray())return data.vArray;
    return nullptr;
}

void* variant::toPointer(bool *ok) const
{
    if(isPointer())
    {
        if(ok)*ok=true;
        return (data.vPointer);
    }
    else if(isData())
    {
        if(ok)*ok=true;
        return (*data.vData)();
    }
    else if(isString())
    {
        if(ok)*ok=true;
        return (*data.vString)();
    }
    else if(isDim())
    {
        if(ok)*ok=true;
        return (*data.vDim)();
    }
    if(ok)*ok=false;
    return NULL;
}

variant variant::neg_value() const
{
    if(type==tReal)
    {
        return -toReal();
    }
    if(type==tInt || type==tBool || type == tString)
    {
        return -toInt();
    }
    if(type == tFraction)
    {
        return -toFraction();
    }
    return variant();
}

variant variant::not_value() const
{
    bool ok = false;
    bool val = toBool(&ok);
    if(!ok)
        return variant();
    return val;
}

variant& variant::operator += (const variant &val)
{
    if(type == tArray && val.type == tArray && val.data.vArray->size() == data.vArray->size())
    {
        for(int i=0;i<data.vArray->size();i++)
        {
            (*data.vArray)[i] += (*val.data.vArray)[i];
        }
        return *this;
    }
    if(type == tHash && val.type == tHash)
    {
        for(int i=0;i<val.data.vHash->size();i++)
        {
            if(!data.vHash->contains(val.data.vHash->key(i)))
                (*data.vHash)[val.data.vHash->key(i)] = val.data.vHash->value(i);
        }
        return *this;
    }
    if(type == tData && val.type == tData)
    {
        data.vData->append(*val.data.vData);
        return *this;
    }

    if(type==tPointer && (val.type==tInt || val.type==tBool || val.type==tReal))
    {
        data.vPointer = ((uint8*)data.vPointer)+val.toInt();
        return *this;
    }

    if(val.type==tPointer && (type==tInt || type==tBool || type==tReal))
    {
        *this = variant(((uint8*)val.data.vPointer)+toInt(),0);
        return *this;
    }

    if((type==tInt || type==tBool) &&
            (val.type==tInt || val.type==tBool))
    {
        *this =  toInt() + val.toInt();
    }
    else if((type==tInt || type==tBool || type==tFraction) &&
            (val.type==tInt || val.type==tBool || val.type==tFraction))
    {
        *this =  toFraction() + val.toFraction();
    }
    else if((type==tReal || type==tInt || type==tBool || type==tFraction) &&
            (val.type==tReal || val.type==tInt || val.type==tBool || val.type==tFraction))
    {
        *this =  toReal() + val.toReal();
    }
    else if((type==tReal || type==tInt || type==tBool || type==tString || type==tData) &&
            (val.type==tReal || val.type==tInt || val.type==tBool || val.type==tString || val.type==tData))
    {
        *this =  toString() + val.toString();
    }
    else
    {
        *this = variant();
    }
    return *this;
}

variant& variant::operator -= (const variant &val)
{
    if(type == tArray && val.type == tArray && val.data.vArray->size() == data.vArray->size())
    {
        for(int i=0;i<data.vArray->size();i++)
        {
            (*data.vArray)[i] -= (*val.data.vArray)[i];
        }
        return *this;
    }
    if(type == tHash && val.type == tHash)
    {
        for(int i=0;i<val.data.vHash->size();i++)
        {
            if(data.vHash->contains(val.data.vHash->key(i)))
                data.vHash->remove(val.data.vHash->key(i));
        }
        return *this;
    }

    if((type==tInt || type==tBool || type==tString) &&
            (val.type==tInt || val.type==tBool || val.type==tString))
    {
        *this =  toInt() - val.toInt();
    }
    else if((type==tInt || type==tBool || type==tFraction) &&
            (val.type==tInt || val.type==tBool || val.type==tFraction))
    {
        *this =  toFraction() - val.toFraction();
    }
    else if((type==tReal || type==tInt || type==tBool || type==tString || type==tFraction) &&
            (val.type==tReal || val.type==tInt || val.type==tBool || val.type==tString || val.type==tFraction))
    {
        *this =  toReal() - val.toReal();
    }
    else
    {
        *this = variant();
    }
    return *this;
}

variant& variant::operator *= (const variant &val)
{
    if(type == tArray && val.type == tArray && val.data.vArray->size() == data.vArray->size())
    {
        for(int i=0;i<data.vArray->size();i++)
        {
            (*data.vArray)[i] *= (*val.data.vArray)[i];
        }
        return *this;
    }
    if((type==tInt || type==tBool || type==tString) &&
            (val.type==tInt || val.type==tBool || val.type==tString))
    {
        *this =  toInt() * val.toInt();
    }
    else if((type==tInt || type==tBool || type==tFraction) &&
            (val.type==tInt || val.type==tBool || val.type==tFraction))
    {
        *this =  toFraction() * val.toFraction();
    }
    else if((type==tReal || type==tInt || type==tBool || type==tString || type==tFraction) &&
            (val.type==tReal || val.type==tInt || val.type==tBool || val.type==tString || val.type==tFraction))
    {
        *this =  toReal() * val.toReal();
    }
    else
    {
        *this = variant();
    }
    return *this;
}

variant& variant::operator /= (const variant &val)
{
    if(type == tArray && val.type == tArray && val.data.vArray->size() == data.vArray->size())
    {
        for(int i=0;i<data.vArray->size();i++)
        {
            (*data.vArray)[i] /= (*val.data.vArray)[i];
        }
        return *this;
    }
    if((type==tInt || type==tBool || type==tString) &&
            (val.type==tInt || val.type==tBool || val.type==tString))
    {
        *this =  toInt() / val.toInt();
    }
    else if((type==tInt || type==tBool || type==tFraction) &&
            (val.type==tInt || val.type==tBool || val.type==tFraction))
    {
        *this =  toFraction() / val.toFraction();
    }
    else if((type==tReal || type==tInt || type==tBool || type==tString || type==tFraction) &&
            (val.type==tReal || val.type==tInt || val.type==tBool || val.type==tString || val.type==tFraction))
    {
        *this =  toReal() / val.toReal();
    }
    else
    {
        *this = variant();
    }
    return *this;
}

variant& variant::operator %= (const variant &val)
{
    if(type == tArray && val.type == tArray && val.data.vArray->size() == data.vArray->size())
    {
        for(int i=0;i<data.vArray->size();i++)
        {
            (*data.vArray)[i] %= (*val.data.vArray)[i];
        }
        return *this;
    }
    if((type==tInt || type==tBool || type==tFraction) &&
                (val.type==tInt || val.type==tBool || val.type==tFraction))
    {
        *this =  toFraction() / val.toFraction();
    }
    else if((type==tReal || type==tInt || type==tBool || type==tString || type==tFraction) &&
            (val.type==tReal || val.type==tInt || val.type==tBool || val.type==tString || val.type==tFraction))
    {
        *this =  toInt() % val.toInt();
    }
    else
    {
        *this = variant();
    }
    return *this;
}

bool variant::operator==(const variant &val) const
{
    if(type!=val.type)
    {
        if(type==tInvalide || val.type==tInvalide)return false;
        if(type==tPointer || val.type==tPointer)return false;
        if(type==tHash || val.type==tHash)return false;
        if(type==tArray || val.type==tArray)return false;

        if((type==tInt && val.type==tBool) ||
                (val.type==tInt && type==tBool))
        {
            return toInt() == val.toInt();
        }

        if((type == tFraction && (val.type == tInt || val.type == tReal || val.type == tBool))
                || (val.type == tFraction && (type == tInt || type == tReal || type == tBool)))
            return toFraction() == val.toFraction();

        if(toString()==val.toString())return true;
        return false;
    }
    if(type==tData && (*data.vData)==(*val.data.vData))return true;
    if(type==tArray && (*data.vArray)==(*val.data.vArray))return true;
    if(type==tHash && (*data.vHash)==(*val.data.vHash))return true;
    if(type==tString && (*data.vString)==(*val.data.vString))return true;
    if(type==tBool && data.vBool==val.data.vBool)return true;
    if(type==tReal && data.vReal==val.data.vReal)return true;
    if(type==tInt && data.vInt==val.data.vInt)return true;
    if(type==tFraction && (*data.vFraction)==(*val.data.vFraction))return true;
    if(type==tDim && (*data.vDim)==(*val.data.vDim))return true;
    if(type==tPointer && data.vPointer==val.data.vPointer)return true;
    if(type==tInvalide)return true;
    return false;
}

bool variant::operator<(const variant &val) const
{
    if(type!=val.type)
    {
        if(type==tInvalide || val.type==tInvalide)return false;
        if(type==tPointer || val.type==tPointer)return false;
        if(type==tHash || val.type==tHash)return false;
        if(type==tArray || val.type==tArray)return false;

        if((type==tReal && (val.type==tInt || val.type==tBool || val.type==tFraction)) ||
                (val.type==tReal && (type==tInt || type==tBool || type==tFraction)))
        {
            return toReal() < val.toReal();
        }

        if(((type==tInt || type==tBool) && val.type==tFraction) ||
                ((val.type==tInt || val.type==tBool) && type==tFraction))
        {
            return toFraction() < val.toFraction();
        }

        if((type==tInt && val.type==tBool) ||
                (val.type==tInt && type==tBool))
        {
            return toInt() < val.toInt();
        }

        if(toString()<val.toString())return true;
        return false;
    }
    if(type==tData && (*data.vData).toHex()<(*val.data.vData).toHex())return true;
    if(type==tString && (*data.vString)<(*val.data.vString))return true;
    if(type==tBool && data.vBool<val.data.vBool)return true;
    if(type==tReal && data.vReal<val.data.vReal)return true;
    if(type==tInt && data.vInt<val.data.vInt)return true;
    if(type==tFraction && (*data.vFraction)<(*val.data.vFraction))return true;
    if(type==tPointer && ptr2int(data.vPointer)<ptr2int(val.data.vPointer))return true;
    return false;
}

bool variant::operator<=(const variant &val) const
{
    if(type!=val.type)
    {
        if(type==tInvalide || val.type==tInvalide)return false;
        if(type==tPointer || val.type==tPointer)return false;
        if(type==tHash || val.type==tHash)return false;
        if(type==tArray || val.type==tArray)return false;

        if((type==tReal && (val.type==tInt || val.type==tBool || val.type==tFraction)) ||
                (val.type==tReal && (type==tInt || type==tBool || type==tFraction)))
        {
            return toReal() <= val.toReal();
        }

        if(((type==tInt || type==tBool) && val.type==tFraction) ||
                ((val.type==tInt || val.type==tBool) && type==tFraction))
        {
            return toFraction() <= val.toFraction();
        }

        if((type==tInt && val.type==tBool) ||
                (val.type==tInt && type==tBool))
        {
            return toInt() <= val.toInt();
        }

        if(toString()<=val.toString())return true;
        return false;
    }
    if(type==tData && (*data.vData).toHex()<=(*val.data.vData).toHex())return true;
    if(type==tString && (*data.vString)<=(*val.data.vString))return true;
    if(type==tBool && data.vBool<=val.data.vBool)return true;
    if(type==tReal && data.vReal<=val.data.vReal)return true;
    if(type==tInt && data.vInt<=val.data.vInt)return true;
    if(type==tFraction && (*data.vFraction)<=(*val.data.vFraction))return true;
    if(type==tPointer && ptr2int(data.vPointer)<=ptr2int(val.data.vPointer))return true;
    return false;
}

variant& variant::operator=(const variant &val)
{
    if(type!=val.type)clear();    
    switch(val.type)
    {
    case tData:
        if(type!=val.type)data.vData=new byteArray;
        *(data.vData)=*(val.data.vData);
        break;
    case tString:
        if(type!=val.type)data.vString=new string;
        *(data.vString)=*(val.data.vString);
        break;
    case tHash:
        if(type!=val.type)data.vHash=new hash<string,variant>;
        *(data.vHash)=*(val.data.vHash);
        break;
    case tArray:
        if(type!=val.type)data.vArray=new array<variant>;
        *(data.vArray)=*(val.data.vArray);
        break;
    case tFraction:
        if(type!=val.type)data.vFraction=new fraction<intx>;
        *(data.vFraction)=*(val.data.vFraction);
        break;
    case tDim:
        if(type!=val.type)data.vDim=new dimensions<uintz>;
        *(data.vDim)=*(val.data.vDim);
        break;
    default:
        data=val.data;
    }
    type=val.type;
    return *this;
}
