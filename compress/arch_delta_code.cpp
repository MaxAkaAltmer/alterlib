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

#include "arch_delta_code.h"
#include "arch_prefix.h"
#include <math.h>
#include <memory.h>

using namespace alt;

class mdwPrefield
{
public:

    mdwPrefield(){size=0;acc=0.0;}

    int size;
    double acc;

    double min(double fird)
    {
        if(fird<acc)return fird;
        return acc;
    }

};

__inline int32 _mdw_read_val(const uint8 *buff, int size, bool big_endian)
{
    int32 rv=0;
    for(int i=0;i<size;i++)
    {
        if(big_endian)
        {
            rv<<=8;
            rv|=buff[i];
        }
        else
        {
            rv|=((int32)buff[i])<<(i*8);
        }
    }    
    return rv;
}

#define MDW_MAX_WORD    sizeof(unsigned int)
MDWFormat _md_FindBestFormat(const uint8 *buff, int size, int endi, int limit)
{
    MDWFormat rv;
    rv.block_size=1;
    rv.big_endian=false;

    if(size<limit*3)
    {
        rv.block_size=0;
        return rv;
    }

    //первый проход - определение размера данных
    alt::array<int> counter(limit);
    counter.fill(0);
    for(int i=limit;i<size-limit;i++)
    {
        for(int j=0;j<limit;j++)
        {
            uint8 tmp=imath::abs(buff[i]-buff[j+i]);
            counter[j]+=tmp;
        }
    }
    //ищем минимум
    rv.block_size=1;
    int curr_cmp=(counter[1]/(size-limit*2));
    for(int i=2;i<limit;i++)
    {
        counter[i]/=size-limit*2;
        if(counter[i]<curr_cmp)
        {
            curr_cmp=counter[i];
            rv.block_size=i;
        }
    }

    //второй проход - определение формата данных и сдвиг блока
    //ищем потенциальные ямы старших байтов
    alt::array<mdwPrefield> tests_big(limit*MDW_MAX_WORD);
    alt::array<mdwPrefield> tests_little(limit*MDW_MAX_WORD);
    alt::array<mdwPrefield> tests_norm(limit);
    alt::array<double> tests_no_delta(limit);

    for(int i=0;i<rv.block_size;i++)
    {
        //единичный подсчет
        tests_norm[i].size=1;
        tests_norm[i].acc=0.0;
        tests_no_delta[i]=0;
        for(int k=rv.block_size;k<size-limit;k+=rv.block_size)
        {
            tests_no_delta[i]+=1<<imath::bsr32(buff[k+i]);
            tests_norm[i].acc+=_pref_sign2unsign(_mdw_delta<int32>(buff[k+i],buff[k+i-rv.block_size],1));
        }
    }

    for(int i=0;i<rv.block_size;i++)
    {        

        for(uint j=0;j<MDW_MAX_WORD-1;j++)
        {
            if(i+j+2>(uint32)rv.block_size)break;
            if(endi<=0 && tests_norm[i+j].acc<=tests_norm[i+j+1].acc)
            {
                //big endian
                tests_big[i*MDW_MAX_WORD+j].acc=0.0;
                tests_big[i*MDW_MAX_WORD+j].size=j+2;
                for(int k=rv.block_size;k<size-limit;k+=rv.block_size)
                {
                    tests_big[i*MDW_MAX_WORD+j].acc+=_pref_sign2unsign(_mdw_delta<int32>(
                        _mdw_read_val(buff+k+i,j+2,true),_mdw_read_val(buff+k+i-rv.block_size,j+2,true),j+2));
                }
            }
            else
            {
                break;
            }
        }
        for(uint j=0;j<MDW_MAX_WORD-1;j++)
        {
            if(i+j+2>(uint32)rv.block_size)break;
            if(endi>=0 && tests_norm[i+j].acc>=tests_norm[i+j+1].acc)
            {
                //little endian
                tests_little[i*MDW_MAX_WORD+j].acc=0.0;
                tests_little[i*MDW_MAX_WORD+j].size=j+2;
                for(int k=rv.block_size;k<size-limit;k+=rv.block_size)
                {
                    tests_little[i*MDW_MAX_WORD+j].acc+=_pref_sign2unsign(_mdw_delta<int32>(
                        _mdw_read_val(buff+k+i,j+2,false),_mdw_read_val(buff+k+i-rv.block_size,j+2,false),j+2));
                }
            }
            else
            {
                break;
            }
        }
    }

    //финальный проход - делаем выводы о формате
    alt::array<mdwPrefield> stack_little(limit);
    int stack_count_little=0;
    double little_full_size=0.0;
    for(int i=0;i<rv.block_size;)
    {
        double best_val=log(tests_norm[i].min(tests_no_delta[i])+1.0)/log(2.0);
        int best_size=1;
        for(uint j=0;j<MDW_MAX_WORD-1;j++)
        {
            if(i+j+2>(uint32)rv.block_size)break;

            if(tests_little[i*MDW_MAX_WORD+j].size)
            {
                //корректируем лучший вариант
                best_val+=log(tests_norm[i+j+1].min(tests_no_delta[i])+1.0)/log(2.0);
                if(best_val>(log(tests_little[i*MDW_MAX_WORD+j].acc+1.0)/log(2.0)))
                {
                    best_val=log(tests_little[i*MDW_MAX_WORD+j].acc+1.0)/log(2.0);
                    best_size=j+2;
                }
            }
            else
            {
                break;
            }
        }
        //запишем лучший вариант
        if(best_size>1)
        {
            stack_little[stack_count_little]=tests_little[i*MDW_MAX_WORD+best_size-2];
        }
        else if(tests_norm[i].acc<tests_no_delta[i])
        {
            stack_little[stack_count_little]=tests_norm[i];
        }
        else
        {
            stack_little[stack_count_little].size=0;
            stack_little[stack_count_little].acc=0.0;
        }
        little_full_size+=log(stack_little[stack_count_little].acc+1.0)/log(2.0);
        stack_count_little++;
        i+=best_size;
    }

    alt::array<mdwPrefield> stack_big(limit);
    int stack_count_big=0;
    double big_full_size=0.0;
    for(int i=0;i<rv.block_size;)
    {
        double best_val=log(tests_norm[i].min(tests_no_delta[i])+1.0)/log(2.0);
        int best_size=1;
        for(uint j=0;j<MDW_MAX_WORD-1;j++)
        {
            if(i+j+2>(uint32)rv.block_size)break;

            if(tests_big[i*MDW_MAX_WORD+j].size)
            {
                //корректируем лучший вариант
                best_val+=log(tests_norm[i+j+1].min(tests_no_delta[i])+1.0)/log(2.0);
                if(best_val>(log(tests_big[i*MDW_MAX_WORD+j].acc+1.0)/log(2.0)))
                {
                    best_val=log(tests_big[i*MDW_MAX_WORD+j].acc+1.0)/log(2.0);
                    best_size=j+2;
                }
            }
            else
            {
                break;
            }
        }
        //запишем лучший вариант
        if(best_size>1)
        {
            stack_big[stack_count_big]=tests_big[i*MDW_MAX_WORD+best_size-2];
        }
        else if(tests_norm[i].acc<tests_no_delta[i])
        {
            stack_big[stack_count_big]=tests_norm[i];
        }
        else
        {
            stack_big[stack_count_big].size=0;
            stack_big[stack_count_big].acc=0.0;
        }
        big_full_size+=log(stack_big[stack_count_big].acc+1.0)/log(2.0);
        stack_count_big++;
        i+=best_size;
    }

    int full_size=0;
    if(big_full_size<little_full_size)
    { //big endian
        rv.big_endian=true;
        for(int i=0;i<stack_count_big;i++)
        {
            full_size+=stack_big[i].size;
            rv.fields.append(stack_big[i].size);
        }
    }
    else
    { //little endian
        rv.big_endian=false;
        for(int i=0;i<stack_count_little;i++)
        {
            full_size+=stack_little[i].size;
            rv.fields.append(stack_little[i].size);
        }
    }
    if(!full_size){rv.block_size=0;rv.fields.clear();}

    return rv;
}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////

__inline void _mdw_write_val(int32 val, uint8 *buff, int size, bool big_endian)
{
    for(int i=0;i<size;i++)
    {
        if(big_endian)
        {
            buff[i]=val>>((size-1-i)*8);
        }
        else
        {
            buff[i]=val>>(i*8);
        }
    }
}

void _md_Encoder(uint8 *buff, int size, const MDWFormat &format, int algo_type)
{
    if(!format.block_size)return;
    int off=0;
    for(int i=0;i<format.fields.size();i++)
    {
        if(format.fields[i])
        {
            MultyDelta<int32> codec;
            codec.Algorithm(algo_type,format.fields[i]);
            for(int j=off;j<=size-format.fields[i];j+=format.block_size)
            {

                //считываем поле и выполняем разностное кодирование
                int32 rez=codec.Encoder(_mdw_read_val(buff+j,format.fields[i],format.big_endian));
                //расширяем знак и переводим в беззнаковую форму
                rez=_pref_sign2unsign(imath::exign(rez,format.fields[i]));
                //запишем результат обратно в буфер
                _mdw_write_val(rez,buff+j,format.fields[i],format.big_endian);
            }
            off+=format.fields[i];
        }
        else
        {
            off++;
        }
    }
}

void _md_Decoder(uint8 *buff, int size, const MDWFormat &format, int algo_type)
{
    if(!format.block_size)return;
    int off=0;
    for(int i=0;i<format.fields.size();i++)
    {
        if(format.fields[i])
        {
            MultyDelta<int32> codec;
            codec.Algorithm(algo_type,format.fields[i]);
            for(int j=off;j<=size-format.fields[i];j+=format.block_size)
            {
                //считываем поле и переводим в знаковую форму
                int32 rez=_mdw_read_val(buff+j,format.fields[i],format.big_endian);
                rez=_pref_unsign2sign(rez);
                //выполняем разностное декодирование
                rez=codec.Decoder(rez);
                //запишем результат обратно в буфер
                _mdw_write_val(rez,buff+j,format.fields[i],format.big_endian);
            }
            off+=format.fields[i];
        }
        else
        {
            off++;
        }
    }
}
