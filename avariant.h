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

#ifndef AVARIANT_H
#define AVARIANT_H

#include "types.h"
#include "at_hash.h"
#include "at_array.h"
#include "adata.h"
#include "astring.h"

//todo: конвертация Real в строку и обратно

class AVariant
{
public:
    AVariant()
    {
        type=tInvalide;
    }
    AVariant(const AVariant &val)
    {
        type=tInvalide;
        *this=val;
    }
    AVariant(bool val)
    {
        type=tBool;
        data.vBool=val;
    }
    AVariant(intx val)
    {
        type=tInt;
        data.vInt=val;
    }
    AVariant(int val)
    {
        type=tInt;
        data.vInt=val;
    }
    AVariant(uintx val)
    {
        type=tInt;
        data.vInt=val;
    }
    AVariant(uint val)
    {
        type=tInt;
        data.vInt=val;
    }
#ifndef ANDROID_NDK
    AVariant(realx val)
    {
        type=tReal;
        data.vReal=val;
    }
#endif
    AVariant(real val)
    {
        type=tReal;
        data.vReal=val;
    }
    AVariant(const AString &val)
    {
        type=tString;
        data.vString=new AString;
        *data.vString=val;
    }
    AVariant(const AData &val)
    {
        type=tData;
        data.vData=new AData;
        *data.vData=val;
    }
    AVariant(const ATHash<AString,AVariant> &val)
    {
        type=tHash;
        data.vHash=new ATHash<AString,AVariant>;
        *data.vHash=val;
    }
    AVariant(const ATArray<AVariant> &val)
    {
        type=tArray;
        data.vArray=new ATArray<AVariant>;
        *data.vArray=val;
    }

    AVariant(const char *val)
    {
        type=tString;
        data.vString=new AString(val);
    }

    AVariant(void *val, int size)
    {
        if(size<=0)
        {
            type=tPointer;
            data.vPointer=val;
        }
        else
        {
            type=tData;
            data.vData=new AData(val,size);
        }
    }

    ~AVariant()
    {
        clear();
    }

    void clear()
    {
        switch(type)
        {
        case tString:
            delete data.vString;
            break;
        case tData:
            delete data.vData;
            break;
        case tHash:
            delete data.vHash;
            break;
        case tArray:
            delete data.vArray;
            break;
        default:
            break;
        };        
        type=tInvalide;
    }

    //////////////////////////////////////////////////////////////////////////////////
    //преобразовалки
    bool toBool(bool *ok=NULL) const;
    intx toInt(bool *ok=NULL) const;
    uintx toUInt(bool *ok=NULL) const {return toInt(ok);}
    realx toReal(bool *ok=NULL) const;
    AData toData(bool *ok=NULL) const;
    AString toString(bool *ok=NULL) const;
    ATHash<AString,AVariant> toHash(bool *ok=NULL) const;
    ATArray<AVariant> toArray(bool *ok=NULL) const;
    ATArray<AVariant>* pointArray();
    void* toPointer(bool *ok=NULL) const;    

    bool isValid() const {return type!=tInvalide;}
    bool isBool() const {return type==tBool;}
    bool isInt() const {return type==tInt;}
    bool isReal() const {return type==tReal;}
    bool isString() const {return type==tString;}
    bool isData() const {return type==tData;}
    bool isHash() const {return type==tHash;}
    bool isArray() const {return type==tArray;}
    bool isPointer() const {return type==tPointer;}

    AVariant& operator=(const AVariant &val);

    bool operator==(const AVariant &val) const;
    bool operator!=(const AVariant &val) const
    {
        return !((*this)==val);
    }

private:

    enum Type
    {
        tInvalide = 0,
        tBool,
        tInt,
        tReal,
        tString,
        tData,
        tHash,
        tArray,
        tPointer
    };

    Type type;
    union
    {
        //стандартные данные
        bool vBool;
        intx vInt;
        realx vReal;
        AString *vString;
        AData *vData;
        ATHash<AString,AVariant> *vHash;
        ATArray<AVariant> *vArray;
        void *vPointer;
    }data;
};

#endif // AVARIANT_H
