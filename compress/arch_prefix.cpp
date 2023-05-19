/*****************************************************************************

This is part of Alterlib - the free code collection under the MIT License
------------------------------------------------------------------------------
Copyright (C) 2006-2021 Maxim L. Grishin  (altmer@arts-union.ru)

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

#include "../amath_int.h"
#include "arch_prefix.h"

using namespace alt;


int _pref_golomb_size(unsigned int val, int p)
{
 int retval=val/p+1;
 int ost=val%p;
 int floor=imath::bsr32(p)-1;
        if(ost<((1<<(floor+1))-p))return retval+floor;
        return retval+floor+1;
}

//------------------------------------------------------------------------------
int _pref_p1qX_size(uint32 val, int q)
{
 int i=1;
        while(val>=(unsigned int)(1<<(q*i)))
        {
                val-=(1<<(q*i));
                i++;
        }
        return i;
}

int _pref_p1qX_encoder(uint64 val, int q, alt::byteArray &buff)
{
 int i=1,j;
        while(val>=((uint64)1<<(q*i)))
        {
                val-=((uint64)1<<(q*i));
                i++;
        }
        for(j=1;j<i;j++)
        {
                buff.append((val&(((uint64)1<<q)-1))|((uint64)1<<q));
                val>>=q;
        }
        buff.append(val);
        return i;
}

int _pref_p1qX_encoder(uint32 val, int q, alt::byteArray &buff)
{
 int i=1,j;
        while(val>=((uint32)1<<(q*i)))
        {
                val-=((uint32)1<<(q*i));
                i++;
        }
        for(j=1;j<i;j++)
        {
                buff.append((val&(((uint32)1<<q)-1))|((uint32)1<<q));
                val>>=q;
        }
        buff.append(val);
        return i;
}

int _pref_p1qX_encoder(uint32 val, int q, alt::array<uint32> &buff)
{
 int i=1,j;
        while(val>=((uint32)1<<(q*i)))
        {
                val-=((uint32)1<<(q*i));
                i++;
        }
        for(j=1;j<i;j++)
        {
                buff.append((val&(((uint32)1<<q)-1))|((uint32)1<<q));
                val>>=q;
        }
        buff.append(val);
        return i;
}

int _pref_p1qX_decoder(uint32 *val, int q, alt::array<uint32> &buff)
{
 int i=0,j;
        *val=0;
        if(!buff.size())return -1;
        while(buff[i]&(1<<q))
        {
                *val|=(buff[i]&((1<<q)-1))<<(i*q);
                i++;
                if(i*q>=32 || buff.size()<=i)return -1;
        }
        *val|=(buff[i]&((1<<q)-1))<<(i*q);
        for(j=0;j<i;j++)*val+=1<<(q*(j+1));
        return i+1;
}

int _pref_p1q7_decoder(uint32 *val, uint8 *buff, int size)
{
    int i=0,j;
    const static int q=7;

           *val=0;
           if(size<=0)return -1;
           while(buff[i]&(1<<q))
           {
                   *val|=(buff[i]&((1<<q)-1))<<(i*q);
                   i++;
                   if(i*q>=32 || size<=i)return -1;
           }
           *val|=(buff[i]&((1<<q)-1))<<(i*q);
           for(j=0;j<i;j++)*val+=1<<(q*(j+1));
           return i+1;
}

int _pref_p1q7_decoder(uint64 *val, uint8 *buff, int size)
{
    int i=0,j;
    const static int q=7;

    *val=0;
    if(size<=0)return -1;
    while(buff[i]&(1<<q))
    {
           *val|=(buff[i]&(((uint64)1<<q)-1))<<(i*q);
           i++;
           if(i*q>=64 || size<=i)return -1;
    }
    *val|=(buff[i]&(((uint64)1<<q)-1))<<(i*q);
    for(j=0;j<i;j++)*val+=(uint64)1<<(q*(j+1));
    return i+1;
}


int _pref_pqs_size(unsigned int val, int p, int q, int s)
{
 unsigned int tmp,rv=0;
 int j,i,k,cnt=0;

        if(s>=q)s=q-1;

        if(s<0)     //отдельный префикс
        {
                if(val<(unsigned int)((1<<(-s))-1)) //когда в его приделах
                        return -s;
                val-=((1<<(-s))-1);  //вычитаем максимум
                rv+=-s;
                s=0;
        }

        if( s )
        {
                //расширение первого интервала
                tmp=((1<<s)-1)*(1<<(q-s)) + (1<<q);
                if(val<tmp) //попали в интервал
                {
                        return p+q;
                }
                else val-=tmp; //первый интервал позади

                for(i=2;i<(1<<p);i++) //для префикса больше одного бита
                {
                        tmp= 1<<(q*i);
                        if( val<tmp ) //попали в интервал
                        {
                                tmp= 1<<(q*i-s);
                                if( val<tmp ) //начало интервала
                                {
                                        rv+=p+q;
                                        val>>=q-s;
                                        for(j=1;j<i;j++)
                                        {
                                                rv+=q;
                                                val>>=q;
                                        }
                                        return rv;
                                }
                                val-=tmp;
                                rv+=p+q;
                                val>>=q-s;
                                val&= (1<<((i-1)*q))-1;
                                for(j=1;j<i;j++)
                                {
                                        rv+=q;
                                        val>>=q;
                                }
                                return rv;
                        }
                        val-=tmp;
                }
                cnt=i;
        }
        else
        {
                cnt=1;
        }

        while( val >= (unsigned int)(1<<(cnt*q-s)) ) //поиск интервала
        {
                val-=(1<<(cnt*q-s));
                cnt++;
        };


        for(i=0;i<cnt;i+=(1<<p)-1)   //финальный парсинг
        {
                k=cnt-i;
                if(k>=(1<<p)) k=(1<<p)-1;
                rv+=p;
                for(j=0;j<k;j++)
                {
                        if(!i && !j)
                                val>>=q-s;
                        else
                                val>>=q;
                        rv+=q;
                }
        }
        return rv;

}

void _pref_pqs_encoder(unsigned int val, alt::bitSpace<uint32> &buff, int p, int q, int s)
{
 unsigned int tmp;
 int j,i,k,cnt=0,smask,mask;

        if(s>=q)s=q-1;

        mask=(1<<q)-1;

        if(s<0)     //отдельный префикс
        {
                if(val<(unsigned int)((1<<(-s))-1)) //когда в его приделах
                {
                        buff.write(val,-s);
                        return;
                }
                val-=((1<<(-s))-1);  //вычитаем максимум
                buff.write(((1<<(-s))-1),-s);
                s=0;
        }


        if( s )
        {
                smask=(1<<(q-s))-1;
                //расширение первого интервала
                tmp=((1<<s)-1)*(1<<(q-s)) + (1<<q);
                if(val<tmp) //попали в интервал
                {
                        if(val<(unsigned int)(1<<q))  //попали в начало интервала?
                        {
                                buff.write(0,p);
                                buff.write(val,q);
                                return;
                        }
                        val-=(1<<q);       //конец интервала
                        buff.write(1,p);
                        buff.write(val,q);
                        return;
                }
                else val-=tmp; //первый интервал позади

                for(i=2;i<(1<<p);i++) //для префикса больше одного бита
                {
                        tmp= 1<<(q*i);
                        if( val<tmp ) //попали в интервал
                        {
                                tmp= 1<<(q*i-s);
                                if( val<tmp ) //начало интервала
                                {
                                        buff.write(i-1,p);
                                        buff.write(val|(~smask),q);
                                        val>>=q-s;
                                        for(j=1;j<i;j++)
                                        {
                                                buff.write(val,q);
                                                val>>=q;
                                        }
                                        return;
                                }
                                val-=tmp;
                                buff.write(i,p);  //конец интервала
                                buff.write( (val&smask) | ( (val>>((i-1)*q))&(~smask) ) ,q);
                                val>>=q-s;
                                val&= (1<<((i-1)*q))-1;
                                for(j=1;j<i;j++)
                                {
                                        buff.write(val,q);
                                        val>>=q;
                                }
                                return;
                        }
                        val-=tmp;
                }
                cnt=i;
        }
        else
        {
                smask=mask;
                cnt=1;
        }

        while( val >= (unsigned int)(1<<(cnt*q-s)) ) //поиск интервала
        {
                val-=(1<<(cnt*q-s));
                cnt++;
        };


        for(i=0;i<cnt;i+=(1<<p)-1)   //финальный парсинг
        {
                k=cnt-i;
                if(k>=(1<<p))
                {
                        k=(1<<p)-1;
                        buff.write(k,p);
                }
                else
                {
                        buff.write(k-1,p);
                }
                for(j=0;j<k;j++)
                {
                        if(!i && !j)
                        {
                                buff.write(val|(~smask),q);
                                val>>=q-s;
                        }
                        else
                        {
                                buff.write(val,q);
                                val>>=q;
                        }
                }
        }

}

unsigned int _pref_pqs_decoder(alt::bitSpace<uint32> &buff, int p, int q, int s)
{
 unsigned int rv=0,pref,suff,tmp=0,i=0,j,k;

        if(s<0)
        {
                tmp=buff.read(-s);
                if( tmp<(unsigned int)((1<<(-s))-1) )return tmp;
                s=0;
        }

        if( s ) //удлиннение интервала
        {
                pref=buff.read(p);
                suff=buff.read(q);
                if( pref==(((unsigned int)1<<p)-1) && (suff>>(q-s))==(((unsigned int)1<<s)-1) ) //за начальным интервалом
                {
                        //корректируем значение
                        rv=suff&((1<<(q-s))-1);
                        for(i=0;i<((unsigned int)1<<p)-1;i++)
                        {
                                if(i)
                                {
                                        suff=buff.read(q);
                                        rv|=suff<<(i*q-s);
                                }
                                tmp+=1<<((i+1)*q);
                        }
                        tmp+=((1<<s)-1)*(1<<(q-s));
                }
                else if(pref==0) //начало первого интервала
                {
                        return suff;
                }
                else if( pref==1 && (suff>>(q-s))!=(unsigned int)((1<<s)-1) ) //конец первого интервала
                {
                        return suff+(1<<q);
                }
                else  //где-то в s-модифицированном интервале
                {
                        tmp=((1<<s)-1)*(1<<(q-s));
                        tmp+=1<<q;
                        if( (suff>>(q-s))!=(((unsigned int)1<<s)-1) )
                        {
                                tmp+=1<<(q*pref-s);
                                tmp+=(suff>>(q-s))<<(q*pref-s);
                        }
                        else pref++;

                        rv=suff&((1<<(q-s))-1);
                        for(i=1;i<pref;i++)
                        {
                                if((i+1)!=pref)tmp+=1<<((i+1)*q);
                                suff=buff.read(q);
                                rv|=suff<<(i*q-s);
                        }
                        return tmp+rv;
                }
        }

        do
        {
                k=pref=buff.read(p);
                if(pref!=(((unsigned int)1<<p)-1))k++;
                for(j=0;j<k;j++,i++)
                {
                        if( !((j+1)==k && pref!=(((unsigned int)1<<p)-1)) )tmp+=1<<((i+1)*q-s);
                        suff=buff.read(q);
                        rv|=suff<<(i*q-s);
                }
        }
        while(pref==(((unsigned int)1<<p)-1));

        return tmp+rv;

}

//------------------------------------------------------------------------------
int _pref_levenstain_size(unsigned int val)
{
 int i;
        val++;
        if(!val)i=33;
        else i=imath::bsr32(val-1);

        if(i==1)return 1;
        return i*2-1;
}
//------------------------------------------------------------------------------
int _pref_omega_size(unsigned int val)
{
 int i,tmp=0;
        val++;
        if(!val)i=33;
        else i=imath::bsr32(val);

        tmp+=i;
        while(i>1)
        {
                i=imath::bsr32(i-1);
                tmp+=i;
        };
        return tmp;
}

void _pref_omega_encoder(unsigned int val, alt::bitSpace<unsigned int> &buff)
{
 int oldpos=buff.position();
 int size=_pref_omega_size(val);
 int tmp,cnt=1;

        buff.setPosition(oldpos+size-cnt);
        buff.write(0,1);

        val++;
        if(!val)  //фикс для переполнения
        {
                cnt+=33;
                val=32;
                buff.setPosition(oldpos+size-cnt);
                buff.write(1,1);
                buff.write(0,32);
        }

        while(val>1)
        {
                cnt+=tmp=imath::bsr32(val);
                buff.setPosition(oldpos+size-cnt);
                buff.write(1,1);  //для корректного декодирования, т.к. в AutoBitSpace младший бит младшему адресу
                buff.write(val,tmp-1);   //пропускаем старший бит
                val=tmp-1;
        }
        buff.setPosition(oldpos+size);
}

unsigned int _pref_omega_decoder(alt::bitSpace<uint32> &buff)
{
 int rv=1;

        while(buff.read(1))
        {
                rv=(1<<rv)+buff.read(rv);
        }
        return rv-1;
}

//------------------------------------------------------------------------------
int _pref_delta_size(unsigned int val)
{
 int i,tmp;
        val++;
        if(!val)i=33;
        else i=imath::bsr32(val);

        tmp=imath::bsr32(i);
        return tmp*2+i-2;
}
