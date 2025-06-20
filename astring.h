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

#ifndef ASTRING_H
#define ASTRING_H

#include "atypes.h"
#include "at_array.h"
#include "at_hash.h"

#ifdef QT_CORE_LIB
    #include <QtCore>
#endif

namespace alt {

    class string
    {
    protected:

        struct Internal
        {
            int size; //размер
            int alloc; //объем выделенной памяти
            int refcount; //число пользователей данной строки
            char buff[1]; //буффер строки
        };

        Internal *data;
        Internal* newInternal(int size)
        {
            Internal *rv;
            int alloc=int(alt::utils::upsize((uint32)size));
            rv=(Internal*)new char[sizeof(Internal)+alloc];
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
            Internal *tmp=newInternal(data->size);
            if(data->size)alt::utils::memcpy(tmp->buff,data->buff,data->size);
            deleteInternal();
            data=tmp;
        }

        static Internal empty;

    public:

    #ifdef QT_CORE_LIB
        string(const QString &str)
        {
            QByteArray tmp=str.toUtf8();
            int size=alt::utils::strlen(tmp.data());
            data=newInternal(size);
            if(size)alt::utils::memcpy(data->buff,tmp.data(),size);
        }
        string& operator=(const QString &str)
        {
            *this=string(str);
            return *this;
        }
        operator QString() const
        {
            return QString::fromUtf8(data->buff);
        }

    #endif

        string()
        {
            data=&empty;
            data->buff[0]=0;
            data->size=0;
            data->alloc=0;
            data->refcount++;
        }

        string(const string &Str)
        {
            //добвляем референс
            data=Str.data;
            data->refcount++;
        }

        string(const char *str)
        {
            int size=alt::utils::strlen(str);
            data=newInternal(size);
            if(size)alt::utils::memcpy(data->buff,str,size);
        }

        explicit string(int size, bool makeEmpty)
        {
            data=newInternal(size);
            if(makeEmpty)
            {
                data->buff[0]=0;
                data->size=0;
            }
        }

        void deepCopy(const string &Str)
        {
            *this = string(Str.data->buff);
        }

        ~string()
        {
            deleteInternal();
        }

        string& clear()
        {
            if(!data->size)return *this;
            cloneInternal();
            data->size=0;
            data->buff[0]=0;
            return *this;
        }

        string& globCorrection()
        {
            //фиксим размер
            int size=alt::utils::strlen(data->buff);
            if(size>data->size)size=data->size;
            data->size=size;
            data->buff[size]=0;
            return *this;
        }

        string& resize(int size)
        {
            if(data->size==size)return *this;

            if(data->alloc<size)
            {
                Internal *tmp=newInternal(size);
                alt::utils::memcpy(tmp->buff,data->buff,data->size);
                deleteInternal();
                data=tmp;
                return *this;
            }

            cloneInternal();
            data->size=size;
            data->buff[size]=0;
            return *this;
        }

        string& reserve(int size)
        {
            if(data->size>=size)return *this;
            Internal *tmp=newInternal(size);
            alt::utils::memcpy(tmp->buff,data->buff,data->size+1);
            tmp->size=data->size;
            deleteInternal();
            data=tmp;
            return *this;
        }

        string& append(char val)
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

            Internal *tmp=newInternal(nsiz);
            if(data->size)alt::utils::memcpy(tmp->buff,data->buff,data->size);
            tmp->buff[data->size]=val;
            deleteInternal();
            data=tmp;
            return *this;
        }

        ////////////////////////////////////////////////////////////////////////
        /// операторы
        ////////////////////////////////////////////////////////////////////////
        string& operator+=(const string &Str);
        string& operator+=(const char *str);
        string& operator=(const string &Str);
        string& operator=(const char *str);

        friend string operator+(const char *str, const string &Str);

        string operator+(const string &Str) const;
        string operator+(const char *str) const;

        bool operator==(const string &Str) const
        {
            if(data->size!=Str.data->size)return false;
            if(data->size==0)return true;
            if(!alt::utils::memcmp(data->buff,Str.data->buff,data->size))return true;
            return false;
        }
        bool operator!=(const string &Str) const
        {
            return !((*this)==Str);
        }

        bool operator<(const string &Str) const
        {
            if(alt::utils::strcmp((unsigned char*)data->buff,(unsigned char*)Str.data->buff)<0)return true;
            return false;
        }
        bool operator<=(const string &Str) const
        {
            if(alt::utils::strcmp((unsigned char*)data->buff,(unsigned char*)Str.data->buff)<=0)return true;
            return false;
        }

        bool operator>(const string &Str) const
        {
            return !((*this)<=Str);
        }
        bool operator>=(const string &Str) const
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

        string trimmed();
        string simplified();
        string spec2space();

        bool contains(char val)
        {
            if(indexOf(val)>=0)return true;
            return false;
        }
        bool contains(const string &val)
        {
            if(indexOf(val)>=0)return true;
            return false;
        }

        int indexOf(const string &val);
        int indexOf(char val, int from=0) const; //-1 if not find
        int countOf(char val);
        int findOneOfChar(int from, char *val) const;
        int findString(int from, char *str) const;
        int findBackChar(int from, char val) const;
        int findBackChar(char val) const {return findBackChar(size()-1,val);}

        string left(int size) const
            {return mid(0,size);}
        string right(int from) const
            {return mid(from,data->size-from);}
        string mid(int from, int size) const;

        string cutPrefix(char sep);
        string cutPrefix(const char *seps);

        string& replace(const string &before, const string &after);

        string& replaceBack(const string &str, int pos);
        string& replaceChar(char seek, char val);
        string& replaceGroup(char seek, char val, int len);
        string& killChar(char val);
        string& killCharAtBegining(char val);

        //разворот строки посимвольно
        string& reverse(int start, int end);
        string& reverse(){return reverse(0,data->size);}
        string& reverseSuffix(int start){return reverse(start,data->size);}
        string& reversePrefix(int end){return reverse(0,end);}

        string& Fill(char sym, int from, int num);

        static string mono(char val, int count)
        {
            string rv;
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

        string& append_unicode(uint32 val)
        {
            if(val&0x80000000)val=0xFFFD;
            int cnt=alt::imath::bsr32(val);
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

        static string fromUTF16(uint16 *str)
        {
            string rv;
            while(*str)
            {
                rv.append_unicode(*str);
                str++;
            }
            return rv;
        }

        static string fromUnicode16(charx *str, int size=-1)
        {
            string rv;
            while(size && (*str))
            {
                rv.append_unicode(*str);
                str++;
                if(size>0)size--;
            }
            return rv;
        }

        static string fromUnicode(const wchar_t *str, int size=-1);

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

        alt::array<charx> toUnicode16() const
        {
            int k,index=0;
            uint32 val;
            alt::array<charx> rv;
            while((k=unicode_at(index,val))!=0)
            {
                rv.append(val);
                index+=k;
            }
            rv.append(0);
            return rv;
        }

        alt::array<wchar_t> toUnicode() const
        {
            int k,index=0;
            uint32 val;
            alt::array<wchar_t> rv;
            while((k=unicode_at(index,val))!=0)
            {
                rv.append(val);
                index+=k;
            }
            rv.append(0);
            return rv;
        }

        string toLower() const;
        static string fromLatin(const char *str);

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

        static string print(const char *format, ... );

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

        static bool isIntegerList(const array<string> &list)
        {
            for(int i=0;i<list.size();i++)
            {
                intx test;
                if(!list[i].tryInt(test))
                    return false;
            }
            return true;
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

        static string fromReal(real val,int prec)
        {
            string format="%."+string::fromInt(prec)+"g";
            return print(format(),val);
        }

        template <class I>
        static string fromInt(I val, int base=10, bool upcase=true)
        {
         const static char low[]="0123456789abcdefghijklmnopqrstuvwxyz???";
         const static char  up[]="0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ???";
         string retval;
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
        static string fromIntFormat(I val, int fix, int base=10, char sym='0', bool upcase=true)
        {
         string retval,tmp;
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

        static string fromFix(const char* fix, int max_size);

        alt::array<string> split(char sym, bool scip_empty=false) const
        {
            alt::array<string> rv;
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

        static string join(const alt::array<string> &arr, char sep)
        {
            string rv;
            for(int i=0;i<arr.size();i++)
            {
                if(i)rv.append(sep);
                rv+=arr[i];
            }
            return rv;
        }

        static string join(const alt::array<string> &arr, string sep)
        {
            string rv;
            for(int i=0;i<arr.size();i++)
            {
                if(i)rv+=sep;
                rv+=arr[i];
            }
            return rv;
        }

        template <class I>
        static string join(const I *arr, uintz size, char sep)
        {
            string rv;
            for(uintz i=0;i<size;i++)
            {
                if(i && sep)rv.append(sep);
                rv+=fromInt(arr[i]);
            }
            return rv;
        }

        template <class T>
        static string poly2string(const array<T> &poly)
        {
            string rv;
            for(intz i = poly.size()-1; i>0; i--)
            {
                if(!poly[i])
                    continue;
                string pow;
                if(i>1)
                    pow = "^"+string::fromInt(i);
                if(poly[i]<0 || !rv.size())
                {
                    if(poly[i] == 1)
                        rv += "x"+pow;
                    else if(poly[i] == -1)
                        rv += "-x"+pow;
                    else
                        rv += string::fromInt(poly[i])+"*x"+pow;
                }
                else
                {
                    if(poly[i] == 1)
                        rv += "+x"+pow;
                    else
                        rv += "+"+string::fromInt(poly[i])+"*x"+pow;
                }
            }
            if(poly.size() && poly[0])
            {
                if(poly[0]<0 || !rv.size())
                    rv += string::fromInt(poly[0]);
                else
                    rv += "+"+string::fromInt(poly[0]);
            }
            return rv;
        }

    };

///////////////////////////////////////////////////////////////////////////////
// Утилиты
///////////////////////////////////////////////////////////////////////////////

    __inline uint32 aHash(const alt::string &key)
    {
        uint32 rv=0;
        for(int i=0;i<key.size();i++)
        {
            rv=(rv>>1)|(rv<<31);
            rv^=key[i];
        }
        return rv;
    }

} // namespace alt

#endif // ASTRING_H
