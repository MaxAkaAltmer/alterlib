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

#ifndef STREAM_DELTA_CODING_HEADER_DEFINITION
#define STREAM_DELTA_CODING_HEADER_DEFINITION

/*****************************************************************************
Примечания:
        Алгоритмы разностного кодирования с уровневой адаптацией разработал
Гришин М.Л.(altmer@arts-union.ru) в 2009г., независимо от возможных
аналогичных решений в данной области.
*****************************************************************************/

#include "../atypes.h"
#include "../amath_int.h"
#include "../at_hash.h"

//функция определения наилучшего формата данных для дельтакода
struct MDWFormat
{
    int block_size;
    bool big_endian;
    alt::array<int> fields;
};
//endi: 0 - all, 1 - little, -1 - big
MDWFormat _md_FindBestFormat(const uint8 *buff, int size, int endi=0, int limit=4);

////////////////////////////////////////////////////////////////
//разность для произвольной разрядности
template <class T>
T _mdw_delta(T v1, T v2, int siz)
{
    T rv=v1-v2;
    if(siz<(int)sizeof(T))
    {
        rv&=((T)1<<(8*siz))-1;
        if( rv&((T)1<<(8*siz-1)) )
        {
            rv|=(~((T)0))<<(8*siz);
        }
    }
    return rv;
}

////////////////////////////////////////////////////////////////
//типы алгоритма
#define MLDLT_NORM      0
#define MLDLT_FAST      1
#define MLDLT_FIX       2

template <class T>
class MultyDelta    //многоуровневый разностный преобразователь потока
{
private:
        int curr_level, max_level, atype, vsize;
        T *delta_stack;

        //нормальня (плавная, пошаговая) уровневая адаптация
        T EncoderNorm(T val)
        {
            int i;
            T tmp,c1,c2,c3;

               for(i=1;i<=curr_level;i++)    //вычисление разности текущего уровня
               {
                   tmp=_mdw_delta(val,delta_stack[i-1],vsize);
                   delta_stack[i-1]=val;
                   val=tmp;
               }

               tmp=_mdw_delta(val,delta_stack[curr_level],vsize);
               if(curr_level<max_level)delta_stack[curr_level+1]=tmp;
               c1=alt::imath::abs(tmp);

               c2=alt::imath::abs(val);
               if(curr_level)c3=alt::imath::abs(delta_stack[curr_level-1]);

               if( c1<c2 && curr_level<max_level && (!curr_level || c1<c3) ) //level up
                   delta_stack[curr_level++]=val;
               else if( curr_level && c3<c2 && (curr_level==max_level || c3<=c1) ) //level down
                   delta_stack[curr_level--]=val;
               else
                   delta_stack[curr_level]=val;
               return val;
        }

        //быстрая (резкая, скачкообразная) уровневая адаптация
        T EncoderFast(T val)
        {
            T tmp, retval=val, delta;
            int i, next_level=0;

               delta=val;
               for(i=1;i<=max_level;i++)    //вычисление разности текущего уровня
               {
                   tmp=_mdw_delta(val,delta_stack[i-1],vsize);
                   delta_stack[i-1]=val;
                   val=tmp;
                   if(i==curr_level)retval=val;
                   if(alt::imath::abs(delta)>alt::imath::abs(val))    //фиксируем оптимальный уровень
                   {
                       delta=val;
                       next_level=i;
                   }
               }
               delta_stack[i-1]=tmp;

               curr_level=next_level;

               return retval;
        }

        //самое обычное фиксированное дельта-кодирование
        T EncoderFix(T val)
        {
            T tmp;
            int i;
               for(i=1;i<=max_level;i++)    //вычисление разности
               {
                   tmp=_mdw_delta(val,delta_stack[i-1],vsize);
                   delta_stack[i-1]=val;
                   val=tmp;
               }
               delta_stack[i-1]=tmp;
               return val;
        }

public:
        MultyDelta()
        {
            atype=MLDLT_NORM;
            curr_level=0;
            max_level=3;
            vsize=sizeof(T);
            delta_stack=new T[max_level+1];
            for(int i=0;i<max_level+1;i++)delta_stack[i]=0;
        }

        ~MultyDelta()
        {
            delete []delta_stack;
        }

        void Algorithm(int type, int size=sizeof(T))
        {
            atype=type; //по умолчанию MLDLT_NORM
            vsize=size; //по умолчанию размер типа
        }
        void Reset(int max, int curr=0)
        {
            int i;
               if(max<1)max=1;
               if(curr>=max)curr=max;
               if(curr<0)curr=0;
               if(max_level!=max)
               {
                   max_level=max;
                   delete []delta_stack;
                   delta_stack=new T[max_level+1];
               }
               for(i=0;i<max_level+1;i++)delta_stack[i]=0;
               curr_level=curr;
        }

        T Encoder(T val)
        {
            switch(atype)
            {
             case MLDLT_NORM:
                    return EncoderNorm(val);
             case MLDLT_FAST:
                    return EncoderFast(val);
             case MLDLT_FIX:
                    return EncoderFix(val);
            };
            return 0;
        }

        T Decoder(T val)
        {
            int i;
               for(i=0;i<curr_level;i++)val+=delta_stack[i];
               if(vsize<(int)sizeof(T))
                   val&=((T)1<<(8*vsize))-1;
               Encoder(val);
               return val;
        }

        int CurrLevel(){return curr_level;}
        T& operator[](int ind){return delta_stack[ind];}  //доступ к разностям

};

////////////////////////////////////////////////////////////////

void _md_Encoder(uint8 *buff, int size, const MDWFormat &format, int algo_type);
void _md_Decoder(uint8 *buff, int size, const MDWFormat &format, int algo_type);

#endif
 
