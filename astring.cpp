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

#include "astring.h"
#include <stdio.h>
#include <stdarg.h>

#include "astring_utf.h"
#include "astring_latin.h"

using namespace alt;

static bool isspace(char val)
{
    if(val=='\t' || val=='\n' || val=='\v' || val=='\f' || val=='\r' || val==' ')return true;
    return false;
}

alt::hash<uint32,uint32> gen_ttab_to_lower()
{
    alt::hash<uint32,uint32> rv;
    for(uint i=0;i<sizeof(as_upper_to_lower)/(sizeof(as_upper_to_lower[0])*2);i++)
        rv.insert(as_upper_to_lower[i*2],as_upper_to_lower[i*2+1]);
    return rv;
}

const static alt::hash<uint32,uint32> ttab_to_lower = gen_ttab_to_lower();

string string::toLower() const
{
    string rv;
    for(int i=0;i<size();)
    {
        uint32 val;
        i+=unicode_at(i,val);
        if(ttab_to_lower.contains(val))
            rv.append_unicode(ttab_to_lower[val]);
        else rv.append_unicode(val);
    }
    return rv;
}

string string::trimmed()
{
    int i;
    for(i=0;i<data->size;i++)
    {
        if(!isspace(data->buff[i]))break;
    }
    int n;
    for(n=data->size-1;n>=0;n--)
    {
        if(!isspace(data->buff[n]))break;
    }
    if(n<i)return string();
    return mid(i,n-i+1);
}

string string::spec2space()
{
    string rv=*this;
    for(int i=0;i<rv.size();i++)
    {
        if(uint8(rv[i])<0x20)rv[i]=' ';
    }
    return rv;
}

string string::simplified()
{
    int i;
    for(i=0;i<data->size;i++)
    {
        if(!isspace(data->buff[i]))break;
    }
    int n;
    for(n=data->size-1;n>=0;n--)
    {
        if(!isspace(data->buff[n]))break;
    }
    if(n<=i)return string();

    string rv(n-i+1,true);
    bool flag=false;
    for(int j=i;j<n+1;j++)
    {
        if(isspace(data->buff[j]))
        {
            if(flag)continue;
            rv.append(' ');
            flag=true;
            continue;
        }
        flag=false;
        rv.append(data->buff[j]);
    }
    return rv;
}

string string::fromLatin(const char *str)
{
    string rv;
    int i=0;
    while(str[i])
    {
        rv.append_unicode(as_latin_table[(uint8)(str[i])]);
        i++;
    }
    return rv;
}

string::Internal string::empty={0,0,0,{0}};

string& string::replace(const string &before, const string &after)
{
    string tmp;
    tmp.reserve(data->size);

    for(int i=0;i<data->size;i++)
    {
        if(data->buff[i]==before.data->buff[0])
        {
            int j=1;
            for(;j<before.data->size && i+j<data->size;j++)
            {
                if(before.data->buff[j]!=data->buff[j+i]) break;
            }
            if(j==before.data->size)
            {
                tmp+=after;
                i+=before.data->size-1;
            }
            else
            {
                tmp.append(data->buff[i]);
            }
        }
        else
        {
            tmp.append(data->buff[i]);
        }
    }
    *this=tmp;
    return *this;
}

string& string::replaceBack(const string &str, int pos)
{
 int i,n,j;

    cloneInternal();

    n=str.size();
    i=0;
    j=pos-n;
    if(j<0){i-=j;j=0;}
    if(pos>size()){n-=pos-size();pos=size();}
    for(;i<n;i++,j++)
    {
            data->buff[j]=str.data->buff[i];
    }
    return *this;
}

string& string::reverse(int start, int end)
{
 char tmp;
 int i,n;

    cloneInternal();

    if(start>end){start+=end;end=start-end;start-=end;}
    if(start<0)start=0;
    if(end>data->size)end=data->size;
    if(start>data->size || start==end || end<0)return *this;
    n=(end-start)/2;
    for(i=0;i<n;i++)
    {
            tmp=data->buff[i+start];
            data->buff[i+start]=data->buff[end-i-1];
            data->buff[end-i-1]=tmp;
    }
    return *this;
}


string& string::replaceChar(char seek, char val)
{
 int i;

    cloneInternal();

    for(i=0;i<data->size;i++)
        if(data->buff[i]==seek)
            data->buff[i]=val;
    return *this;
}

string& string::replaceGroup(char seek, char val, int len)
{
    cloneInternal();

    for(int i=0;i<data->size;i++)
    {
        if( ((unsigned char)data->buff[i])>=((unsigned char)seek)
            && ((unsigned char)data->buff[i])<((unsigned char)(seek+len)))
            data->buff[i]+=val-seek;
    }
    return *this;
}

string& string::killCharAtBegining(char val)
{
 int i,pnt;
 bool beg=true;

    cloneInternal();
    pnt=0;
    for(i=0;i<data->size;i++)
    {
       if(data->buff[i]!=val || !beg){data->buff[pnt++]=data->buff[i];beg=false;}
    }
    data->size=pnt;
    data->buff[pnt]=0;
    return *this;
}

string& string::killChar(char val)
{
 int i,pnt;
    pnt=0;

    cloneInternal();

    for(i=0;i<data->size;i++)
    {
            if(data->buff[i]!=val)data->buff[pnt++]=data->buff[i];
    }
    data->size=pnt;
    data->buff[pnt]=0;
    return *this;
}

int string::indexOf(const string &val)
{
    if(val.isEmpty() || !data || data->size<val.data->size)return -1;

    char hash=0,shablon=0;
    for(int i=0;i<val.data->size;i++)
    {
        hash^=data->buff[i];
        shablon^=val.data->buff[i];
    }

    for(int i=0;i<data->size-val.size()+1;i++)
    {
        if(hash==shablon)
        {
            if(!alt::utils::memcmp(&data->buff[i],val.data->buff,val.data->size))
            {
                return i;
            }
        }
        hash^=data->buff[i];
        hash^=data->buff[i+val.data->size];
    }
    return -1;
}

int string::countOf(char val)
{
    int rv=0;
    for(int i=0;i<data->size;i++)
        if((char)data->buff[i]==val)rv++;
    return rv;
}

int string::indexOf(char val, int from) const
{
    if(from<0)from=0;
    while(from<data->size)
    {
        if((char)data->buff[from]==val)return from;
        from++;
    }
    return -1;
}


int string::findOneOfChar(int from, char *val) const
{
 int j;
    if(from<0)from=0;
    while(from<data->size)
    {
        j=0;
        while(val[j])
        {
                if((char)data->buff[from]==val[j])return from;
                j++;
        }
        from++;
    }
    return -1;
}


string string::cutPrefix(char sep)
{
 string retval;
 int i;
    for(i=0;i<data->size;i++)
    {
            if(data->buff[i]==sep)break;
    }
    retval=this->left(i);
    *this=this->right(i+1);
    return retval;
}


string string::cutPrefix(const char *seps)
{
 string retval;
 int i,j,n=alt::utils::strlen(seps);
        for(i=0;i<data->size;i++)
        {
                for(j=0;j<n;j++)if(data->buff[i]==seps[j])break;
                if(j!=n)break;
        }
        retval=this->left(i);
        *this=this->right(i+1);
        return retval;
}


int string::findString(int from, char *str) const
{
 int len=alt::utils::strlen(str);
        if(from<0)from=0;
        while(data->size>=from+len)
        {
                if(!alt::utils::memcmp(&data->buff[from],str,len))return from;
                from++;
        }
        return -1;
}


int string::findBackChar(int from, char val) const
{
        if(from>=data->size)from=data->size-1;
        while(from>=0)
        {
                if(data->buff[from]==val)return from;
                from--;
        }
        return from;
}


string string::mid(int from, int size) const
{
 string retval;

    if(from<0){size+=from;from=0;}
    if(from>=data->size || size<=0)return retval;
    if(from+size>data->size)size=data->size-from;

    retval.resize(size);
    alt::utils::memcpy(retval.data->buff,&data->buff[from],size);
    return retval;
}



string& string::operator+=(const string &Str)
{
    int nsiz=Str.data->size+data->size;;

    if(!Str.data->size)return *this;

    if(data->alloc>=nsiz && data->refcount<2)
    {
        alt::utils::memcpy(&data->buff[data->size],Str.data->buff,Str.data->size);
        data->size=nsiz;
        data->buff[nsiz]=0;
        return *this;
    }

    Internal *tmp=newInternal(nsiz);
    if(data->size)alt::utils::memcpy(tmp->buff,data->buff,data->size);
    alt::utils::memcpy(&tmp->buff[data->size],Str.data->buff,Str.data->size);
    deleteInternal();
    data=tmp;
    return *this;
}


string& string::operator+=(const char *str)
{
    int size=alt::utils::strlen(str);

    if(!size)return *this;
    int nsiz=size+data->size;

    if(data->alloc>=nsiz && data->refcount<2)
    {
        alt::utils::memcpy(&data->buff[data->size],str,size);
        data->size=nsiz;
        data->buff[nsiz]=0;
        return *this;
    }

    Internal *tmp=newInternal(nsiz);
    if(data->size)alt::utils::memcpy(tmp->buff,data->buff,data->size);
    alt::utils::memcpy(&tmp->buff[data->size],str,size);
    deleteInternal();
    data=tmp;
    return *this;
}


string& string::operator=(const string &Str)
{
    if(data==Str.data)return *this;
    if((data->refcount<2) && (data->alloc >= Str.data->size) )
    {
        alt::utils::memcpy(data->buff,Str.data->buff,Str.data->size+1);
        data->size=Str.data->size;
        return *this;
    }
    deleteInternal();
    data=Str.data;
    data->refcount++;
    return *this;
}


string& string::operator=(const char *str)
{
    int size=alt::utils::strlen(str);

    if(data->refcount<2 && data->alloc>=size)
    {
        alt::utils::memcpy(data->buff,str,size+1);
        data->size=size;
        return *this;
    }

    deleteInternal();
    data=newInternal(size);
    if(size)alt::utils::memcpy(data->buff,str,size);
    return *this;
}


string alt::operator+(const char *str, const string &Str)
{
    int size=alt::utils::strlen(str);
    int nsiz=size+Str.size();

    if(!size) return Str;
    if(!Str.size()) return string(str);

    string tmp(nsiz,false);
    alt::utils::memcpy(tmp(),str,size);
    alt::utils::memcpy(&tmp()[size],Str(),Str.size());
    return tmp;
}


string string::operator+(const string &Str)  const
{
    if(!Str.data->size)return *this;
    if(!data->size)return Str;
    int nsiz=Str.data->size+data->size;

    string tmp(nsiz,false);
    alt::utils::memcpy(tmp.data->buff,data->buff,data->size);
    alt::utils::memcpy(&tmp.data->buff[data->size],Str.data->buff,Str.data->size);
    return tmp;
}


string string::operator+(const char *str) const
{
    int size=alt::utils::strlen(str);
    if(!size)return *this;
    if(!data->size)return string(str);
    int nsiz=size+data->size;

    string tmp(nsiz,false);
    alt::utils::memcpy(tmp.data->buff,data->buff,data->size);
    alt::utils::memcpy(&tmp.data->buff[data->size],str,size);
    return tmp;
}


string& string::Fill(char sym, int from, int num)
{
    cloneInternal();

    if(data->size<from+num)resize(from+num);

    for(int i=0;i<num;i++)
    {
        data->buff[i+from]=sym;
    }
    globCorrection();

    return *this;
}

string string::print(const char *format, ... )
{
    va_list argptr;
    int cnt;
    string rv;

    va_start(argptr, format);
    cnt=vsnprintf(NULL, 0, format, argptr);
    va_end(argptr);

    rv.resize(cnt);
    va_start(argptr, format);
    rv.data->size=vsnprintf(rv(), rv.Allocated(), format, argptr);
    va_end(argptr);

    return rv;
}

string string::fromUnicode(const wchar_t *str, int size)
{
    string rv;
    while(size && (*str))
    {
        if(sizeof(wchar_t)<sizeof(uint32))
            rv.append_unicode(((uint32)(*str))& ((((uint32)1)<<(sizeof(wchar_t)*8))-1) );
        else rv.append_unicode(*str);
        str++;
        if(size>0)size--;
    }
    return rv;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------


string string::fromFix(const char* fix, int max_size)
{
 string retval;
 char *pnt;

    retval.resize(max_size);
    pnt=retval();
    alt::utils::memcpy(pnt,fix,max_size);
    retval.globCorrection();
    return retval;
}

