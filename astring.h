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

#ifndef ASTRING_H
#define ASTRING_H

#include "types.h"
#include "at_string_utils.h"
#include "at_array.h"
#include "at_hash.h"

#ifdef QT_CORE_LIB
    #include <QtCore>
#endif

struct AStringInternal
{
    int size; //размер
    int alloc; //объем выделенной памяти
    int refcount; //число пользователей данной строки
    char buff[1]; //буффер строки
};

class AString
{
protected:

    AStringInternal *data;
    AStringInternal* newInternal(int size)
    {
        AStringInternal *rv;
        int alloc=_ats_upsize((uint32)size);
        rv=(AStringInternal*)new char[sizeof(AStringInternal)+alloc];
        rv->alloc=alloc;
        rv->refcount=1;
        rv->size=size;
        rv->buff[size]=0;
        return rv;
    }

    void deleteInternal()
    {
        data->refcount--;
        if(data==&empty)return;
        if( !data->refcount )
            delete []((char*)data);
    }

    void cloneInternal()
    {
        if( data!=(&empty) && data->refcount<2)return;
        AStringInternal *tmp=newInternal(data->size);
        if(data->size)_ats_memcpy(tmp->buff,data->buff,data->size);
        deleteInternal();
        data=tmp;
    }

    static AStringInternal empty;

public:

#ifdef QT_CORE_LIB
    AString(const QString &str)
    {
        QByteArray tmp=str.toUtf8();
        int size=_ats_strlen(tmp.data());
        data=newInternal(size);
        if(size)_ats_memcpy(data->buff,tmp.data(),size);
    }
    AString& operator=(const QString &str)
    {
        *this=AString(str);
        return *this;
    }
    operator QString()
    {
        return QString::fromUtf8(data->buff);
    }

#endif

    AString()
    {
    	data=&empty;
    	data->buff[0]=0;
    	data->size=0;
    	data->alloc=0;
    	data->refcount++;
    }

    AString(const AString &Str)
    {
        //добвляем референс
        data=Str.data;
        data->refcount++;
    }

    AString(const char *str)
    {
        int size=_ats_strlen(str);
        data=newInternal(size);
        if(size)_ats_memcpy(data->buff,str,size);
    }

    AString(int size, bool makeEmpty)
    {
        data=newInternal(size);
        if(makeEmpty)
        {
            data->buff[0]=0;
            data->size=0;
        }
    }

    void deepCopy(const AString &Str)
    {
        *this = AString(Str.data->buff);
    }

    ~AString()
    {
        deleteInternal();
    }

    AString& clear()
    {
        if(!data->size)return *this;
        cloneInternal();
        data->size=0;
        data->buff[0]=0;
        return *this;
    }

    AString& globCorrection()
    {
        //фиксим размер
        int size=_ats_strlen(data->buff);
        if(size>data->size)size=data->size;
        data->size=size;
        data->buff[size]=0;
        return *this;
    }

    AString& resize(int size)
    {
        if(data->size==size)return *this;

        if(data->alloc<size)
        {
            AStringInternal *tmp=newInternal(size);
            _ats_memcpy(tmp->buff,data->buff,data->size);
            deleteInternal();
            data=tmp;
            return *this;
        }

        cloneInternal();
        data->size=size;
        data->buff[size]=0;
        return *this;
    }

    AString& reserve(int size)
    {
        if(data->size>=size)return *this;
        AStringInternal *tmp=newInternal(size);
        _ats_memcpy(tmp->buff,data->buff,data->size+1);
        tmp->size=data->size;
        deleteInternal();
        data=tmp;
        return *this;
    }

    AString& append(char val)
    {
        if(!val)return *this;
        int nsiz=data->size+1;

        if(data->alloc>=nsiz && data->refcount<2)
        {
            data->buff[data->size]=val;
            data->size=nsiz;
            data->buff[nsiz]=0;
            return *this;
        }

        AStringInternal *tmp=newInternal(nsiz);
        if(data->size)_ats_memcpy(tmp->buff,data->buff,data->size);
        tmp->buff[data->size]=val;
        deleteInternal();
        data=tmp;
        return *this;
    }

    ////////////////////////////////////////////////////////////////////////
    /// операторы
    ////////////////////////////////////////////////////////////////////////
    AString& operator+=(const AString &Str);
    AString& operator+=(const char *str);
    AString& operator=(const AString &Str);
    AString& operator=(const char *str);

    friend AString operator+(const char *str, const AString &Str);

    AString operator+(const AString &Str) const;
    AString operator+(const char *str) const;

    bool operator==(const AString &Str) const
    {
        if(data->size!=Str.data->size)return false;
        if(data->size==0)return true;
        if(!_ats_memcmp(data->buff,Str.data->buff,data->size))return true;
        return false;
    }
    bool operator!=(const AString &Str) const
    {
        return !((*this)==Str);
    }

    bool operator<(const AString &Str) const
    {
        if(_ats_strcmp((unsigned char*)data->buff,(unsigned char*)Str.data->buff)<0)return true;
        return false;
    }
    bool operator<=(const AString &Str) const
    {
        if(_ats_strcmp((unsigned char*)data->buff,(unsigned char*)Str.data->buff)<=0)return true;
        return false;
    }

    bool operator>(const AString &Str) const
    {
        return !((*this)<=Str);
    }
    bool operator>=(const AString &Str) const
    {
        return !((*this)<Str);
    }

    //получить ссылку на буфер
    const char* operator()() const
    {
        return data->buff;
    }
    char* operator()()
    {
        cloneInternal();
        return data->buff;
    }

    //работа с символами
    char& operator[](int ind)
    {
        cloneInternal();
        return data->buff[ind];
    }
    char operator[](int ind) const
    {
        return data->buff[ind];
    }

    bool isEmpty() const       //проверка на пустоту
    {
        return !data->size;
    }

    bool isCppName() const
    {
        if(isEmpty())return false;
        for(int j=0;j<data->size;j++)
        {
            if((data->buff[j]>='a' && data->buff[j]<='z') || (data->buff[j]>='A' && data->buff[j]<='Z') || data->buff[j]=='_')
                continue;
            if(j && (data->buff[j]>='0' && data->buff[j]<='9'))
                continue;

            return false;
        }
        return true;
    }

    int size() const
            { return data->size; }
    int Allocated() const      //размер выделенной памяти
            { return data->alloc; }

    AString trimmed();
    AString simplified();
    AString spec2space();

    bool contains(char val)
    {
        if(indexOf(val)>=0)return true;
        return false;
    }
    bool contains(const AString &val)
    {
        if(indexOf(val)>=0)return true;
        return false;
    }

    int indexOf(const AString &val);
    int indexOf(char val, int from=0) const; //-1 if not find
    int countOf(char val);
    int findOneOfChar(int from, char *val) const;
    int findString(int from, char *str) const;
    int findBackChar(int from, char val) const;
    int findBackChar(char val) const {return findBackChar(size()-1,val);}

    AString left(int size) const
        {return mid(0,size);}
    AString right(int from) const
        {return mid(from,data->size-from);}
    AString mid(int from, int size) const;

    AString cutPrefix(char sep);
    AString cutPrefix(const char *seps);

    AString& replace(const AString &before, const AString &after);

    AString& ReplaceBack(const AString &str, int pos);
    AString& ReplaceChar(char seek, char val);
    AString& ReplaceGroup(char seek, char val, int len);
    AString& KillChar(char val);
    AString& KillCharAtBegining(char val);

    //разворот строки посимвольно
    AString& reverse(int start, int end);
    AString& reverse(){return reverse(0,data->size);}
    AString& reverseSuffix(int start){return reverse(start,data->size);}
    AString& reversePrefix(int end){return reverse(0,end);}

    AString& Fill(char sym, int from, int num);

    static AString mono(char val, int count)
    {
        AString rv;
        rv.Fill(val,0,count);
        return rv;
    }

    /////////////////////////////////////////////////////////
    int unicode_at(int posit, uint32 &val) const
    {
        if(size()<=posit)return 0;

        if(!(data->buff[posit]&0x80))
        {
            val=data->buff[posit];
            return 1;
        }
        val=0xFFFD;
        if((data->buff[posit]&0xC0)==0x80)return 1;

        const static int mask[]={0xC0,0xE0,0xF0,0xF8,0xFC,0xFE};
        for(int k=0;k<5;k++)
        {
            if((data->buff[posit]&mask[k+1])!=mask[k])continue;
            if(size()<=posit+k+1)return 1;
            uint32 tmp=((data->buff[posit]&(~mask[k+1]))&0xff)<<((k+1)*6);
            for(int i=0;i<=k;i++)
            {
                if((data->buff[posit+i+1]&0xC0)!=0x80)return 1;
                if(i==k) tmp|=data->buff[posit+i+1]&0x3f;
                else tmp|=(data->buff[posit+i+1]&0x3f)<<((k-i)*6);
            }
            val=tmp;
            return k+2;
        }
        return 1;
    }

    AString& append_unicode(uint32 val)
    {
        if(val&0x80000000)val=0xFFFD;
        int cnt=__bsr32(val);
        if(cnt<=7)
        {
            append(val);
        }
        else if(cnt<=11)
        {
            append(0xC0|(val>>6));
            append(0x80|(val&0x3F));
        }
        else if(cnt<=16)
        {
            append(0xE0|(val>>12));
            append(0x80|((val>>6)&0x3F));
            append(0x80|(val&0x3F));
        }
        else if(cnt<=21)
        {
            append(0xF0|(val>>18));
            append(0x80|((val>>12)&0x3F));
            append(0x80|((val>>6)&0x3F));
            append(0x80|(val&0x3F));
        }
        else if(cnt<=26)
        {
            append(0xF8|(val>>24));
            append(0x80|((val>>18)&0x3F));
            append(0x80|((val>>12)&0x3F));
            append(0x80|((val>>6)&0x3F));
            append(0x80|(val&0x3F));
        }
        else
        {
            append(0xFC|(val>>30));
            append(0x80|((val>>24)&0x3F));
            append(0x80|((val>>18)&0x3F));
            append(0x80|((val>>12)&0x3F));
            append(0x80|((val>>6)&0x3F));
            append(0x80|(val&0x3F));
        }
        return *this;
    }

    static AString fromUTF16(uint16 *str)
    {
        AString rv;
        while(*str)
        {
            rv.append_unicode(*str);
            str++;
        }
        return rv;
    }

    static AString fromUnicode16(charx *str, int size=-1)
    {
        AString rv;
        while(size && (*str))
        {
            rv.append_unicode(*str);
            str++;
            if(size>0)size--;
        }
        return rv;
    }

    static AString fromUnicode(wchar_t *str, int size=-1);

    int unicodeSize() const
    {
        int k,index=0;
        uint32 val;
        int rv=0;
        while((k=unicode_at(index,val))!=0)
        {
            rv++;
            index+=k;
        }
        return rv;
    }

    ATArray<charx> toUnicode16() const
    {
        int k,index=0;
        uint32 val;
        ATArray<charx> rv;
        while((k=unicode_at(index,val))!=0)
        {
            rv.append(val);
            index+=k;
        }
        rv.append(0);
        return rv;
    }

    ATArray<wchar_t> toUnicode() const
    {
        int k,index=0;
        uint32 val;
        ATArray<wchar_t> rv;
        while((k=unicode_at(index,val))!=0)
        {
            rv.append(val);
            index+=k;
        }
        rv.append(0);
        return rv;
    }

    AString toLower() const;
    static AString fromLatin(const char *str);

    /////////////////////////////////////////////////////////
    char at(int index) const
    {
        if(size()<=index)return 0;
        return data->buff[index];
    }
    char last() const
    {
        if(!size())return 0;
        return data->buff[data->size-1];
    }

    static AString print(const char *format, ... );

    template <class I> bool tryInt(I &val) const
    {
        bool ok=false;
        if(at(0)=='#')
        {
            val=right(1).toInt<I>(16,&ok);
            return ok;
        }
        else if(last()=='h' || last()=='H')
        {
            val=left(size()-1).toInt<I>(16,&ok);
            return ok;
        }
        else if(at(0)=='0' && (at(1)=='x' || at(1)=='X'))
        {
            val=right(2).toInt<I>(16,&ok);
            return ok;
        }
        else if(last()=='b' || last()=='B')
        {
            val=left(size()-1).toInt<I>(2,&ok);
            return ok;
        }
        else if(at(0)=='0' && (at(1)=='b' || at(1)=='B'))
        {
            val=right(2).toInt<I>(2,&ok);
            return ok;
        }
        val=toInt<I>(10,&ok);
        return ok;
    }

    template <class I> I toInt(int base=10, bool *ok=NULL) const
    {
        I rv=0;
        bool neg=false;
        if(ok)*ok=false;

        if(base<2)return 0;
        if(base>36)return 0;

        if(!data || !data->size)return 0;
        for(int i=0;i<data->size;i++)
        {
            if(data->buff[i]>='0' && data->buff[i]<='9')
            {
                rv*=base;
                int inc=data->buff[i]-'0';
                if(inc>=base)
                {
                    if(neg)return -rv;
                    return rv;
                }
                rv+=inc;
            }
            else if(data->buff[i]>='a' && data->buff[i]<='z')
            {
                rv*=base;
                int inc=data->buff[i]-'a'+10;
                if(inc>=base)
                {
                    if(neg)return -rv;
                    return rv;
                }
                rv+=inc;
            }
            else if(data->buff[i]>='A' && data->buff[i]<='Z')
            {
                rv*=base;
                int inc=data->buff[i]-'A'+10;
                if(inc>=base)
                {
                    if(neg)return -rv;
                    return rv;
                }
                rv+=inc;
            }
            else if(data->buff[i]=='-' && !i)
            {
                neg=true;
            }
            else if(data->buff[i]=='+' && !i)
            {
                continue;
            }
            else
            {
                if(neg)return -rv;
                return rv;
            }
        }

        if(ok)*ok=true;
        if(neg)return -rv;
        return rv;
    }

    template <class R>
    R toReal() //todo: сделать поддержку расширенных форм
    {
        R rv=0.0, div=1.0;
        bool neg=false;
        bool afterpoint=false;

        if(!data)return 0.0;
        for(int i=0;i<data->size;i++)
        {
            if(data->buff[i]=='-')
            {
                neg=true;
            }
            else if(data->buff[i]>='0' && data->buff[i]<='9')
            {
                if(afterpoint)div*=10.0;
                rv*=10.0;
                rv+=data->buff[i]-'0';
            }
            else if(data->buff[i]=='.')
            {
                afterpoint=true;
            }
        }
        rv/=div;
        if(neg)return -rv;
        return rv;
    }

    ///////////////////////////////////////////////////////////////////////////

    static AString fromReal(real val,int prec)
    {
        AString format="%."+AString::fromInt(prec)+"g";
        return print(format(),val);
    }

    template <class I>
    static AString fromInt(I val, int base=10, bool upcase=true)
    {
     const static char low[]="0123456789abcdefghijklmnopqrstuvwxyz???";
     const static char  up[]="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ???";
     AString retval;
     const char *pnt;

            if(!val)return "0";
            if(base<2)base=2;
            else if(base>36)base=36;
            if(upcase)pnt=up;
            else pnt=low;
            if(val<0)
            {
                    retval.append('-');
                    retval.append(pnt[-(val%base)]);
                    val/=base;
                    val=-val;
                    if(val==0)return retval;
            }
            while(val)
            {
                    retval.append(pnt[val%base]);
                    val/=base;
            }
            if(retval[0]=='-')retval.reverseSuffix(1);
            else retval.reverse();
            return retval;
    }

    template <class I>
    static AString fromIntFormat(I val, int fix, int base=10, char sym='0', bool upcase=true)
    {
     AString retval,tmp;
     int i,lim,j;

            if(fix<=0)fix=1;
            retval.Fill('0',0,fix);

            tmp=fromInt(val,base,upcase);

            if(tmp.size()>=fix)return tmp;

            if(tmp[0]=='-')lim=1;
            else lim=0;
            for(j=retval.size()-1,i=tmp.size()-1;i>=lim;i--,j--)
            {
                    retval[j]=tmp[i];
            }
            if(lim)retval[0]='-';

            return retval;
    }

    static AString fromFix(const char* fix, int max_size);

    ATArray<AString> split(char sym, bool scip_empty=false) const
    {
        ATArray<AString> rv;
        if(!data)return rv;

        int curr=0;
        for(int i=0;i<data->size;i++)
        {
            if(data->buff[i]==sym)
            {
                if(!scip_empty || i!=curr)
                    rv.append(fromFix(&data->buff[curr],i-curr));
                curr=i+1;
            }
        }
        if(curr<data->size || !scip_empty)
        {
            rv.append(right(curr));
        }
        return rv;
    }

    static AString join(const ATArray<AString> &arr, char sep)
    {
        AString rv;
        for(int i=0;i<arr.size();i++)
        {
            if(i)rv.append(sep);
            rv+=arr[i];
        }
        return rv;
    }

};

///////////////////////////////////////////////////////////////////////////////
// Утилиты
///////////////////////////////////////////////////////////////////////////////

__inline uint32 aHash(const AString &key)
{
    uint32 rv=0;
    for(int i=0;i<key.size();i++)
    {
        rv=(rv>>1)|(rv<<31);
        rv^=key[i];
        i++;
    }
    return rv;
}

AString at(AString text);
ATHash<AString,AString> aGetTranslation();
void aApplayTranslation(ATHash<AString,AString> transl);

#endif // ASTRING_H
