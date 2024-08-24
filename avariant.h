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

#ifndef AVARIANT_H
#define AVARIANT_H

#include "atypes.h"
#include "astring.h"
#include "at_array.h"
#include "abyte_array.h"
#include "at_fraction.h"

namespace alt {

    class variant
    {
    public:
        variant()
        {
            type=tInvalide;
        }
        variant(const variant &val)
        {
            type=tInvalide;
            *this=val;
        }
        variant(bool val)
        {
            type=tBool;
            data.vBool=val;
        }
        variant(intx val)
        {
            type=tInt;
            data.vInt=val;
        }
        variant(int val)
        {
            type=tInt;
            data.vInt=val;
        }
        variant(uintx val)
        {
            type=tInt;
            data.vInt=val;
        }
        variant(long unsigned int val)
        {
            type=tInt;
            data.vInt=val;
        }
        variant(uint val)
        {
            type=tInt;
            data.vInt=val;
        }
        variant(fraction<intx> val)
        {
            type=tFraction;
            data.vFraction = new fraction<intx>;
            *data.vFraction = val;
        }
    #ifndef ANDROID_NDK
        variant(realx val)
        {
            type=tReal;
            data.vReal=val;
        }
    #endif
        variant(real val)
        {
            type=tReal;
            data.vReal=val;
        }
        variant(const string &val)
        {
            type=tString;
            data.vString=new string;
            *data.vString=val;
        }
        variant(const byteArray &val)
        {
            type=tData;
            data.vData=new byteArray;
            *data.vData=val;
        }
        variant(const hash<string,variant> &val)
        {
            type=tHash;
            data.vHash=new hash<string,variant>;
            *data.vHash=val;
        }
        variant(const array<variant> &val)
        {
            type=tArray;
            data.vArray=new array<variant>;
            *data.vArray=val;
        }

        variant(const char *val)
        {
            type=tString;
            data.vString=new string(val);
        }

        variant(void *val, int size)
        {
            if(size<=0)
            {
                type=tPointer;
                data.vPointer=val;
            }
            else
            {
                type=tData;
                data.vData=new byteArray(val,size);
            }
        }

        ~variant()
        {
            clear();
        }

        void clear()
        {
            switch(type)
            {
            case tFraction:
                delete data.vFraction;
                break;
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
        byteArray toData(bool *ok=NULL) const;
        string toString(bool *ok=NULL) const;
        fraction<intx> toFraction(bool *ok=NULL) const;
        hash<string, variant> toHash(bool *ok=NULL) const;
        array<variant> toArray(bool *ok=NULL) const;
        array<variant>* pointArray();
        void* toPointer(bool *ok=NULL) const;

        uintx size() const;

        template<class X>
        X deserialize() const
        {
            X rv;
            if(isData() && sizeof(X)==data.vData->size())
               utils::memcpy((uint8*)&rv,(*data.vData)(),sizeof(X));
            return rv;
        }

        bool isValid() const {return type!=tInvalide;}
        bool isBool() const {return type==tBool;}
        bool isInt() const {return type==tInt;}
        bool isFraction() const {return type==tFraction;}
        bool isReal() const {return type==tReal;}
        bool isString() const {return type==tString;}
        bool isData() const {return type==tData;}
        bool isHash() const {return type==tHash;}
        bool isArray() const {return type==tArray;}
        bool isPointer() const {return type==tPointer;}

        variant& operator=(const variant &val);

        bool operator==(const variant &val) const;
        bool operator!=(const variant &val) const
        {
            return !((*this)==val);
        }

        bool operator<(const variant &val) const;
        bool operator>=(const variant &val) const
        {
            return !((*this)<val);
        }

        bool operator<=(const variant &val) const;
        bool operator>(const variant &val) const
        {
            return !((*this)<=val);
        }

        variant& operator += (const variant &val);
        variant& operator -= (const variant &val);
        variant& operator *= (const variant &val);
        variant& operator /= (const variant &val);
        variant& operator %= (const variant &val);

        variant neg_value() const;
        variant not_value() const;

    private:

        enum Type
        {
            tInvalide = 0,
            tBool,
            tInt,
            tFraction,
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
            //todo: изменить с указателей на поля с явным возовом конструкторов и деструкторов
            //стандартные данные
            bool vBool;
            intx vInt;
            realx vReal;
            fraction<intx> *vFraction;
            string *vString;
            byteArray *vData;
            hash<string,variant> *vHash;
            array<variant> *vArray;
            void *vPointer;
        }data;
    };

} // namespace alt

#endif // AVARIANT_H
