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

#include "arch_diction.h"
#include "arch_prefix.h"

void _arch_diction_encode(AData &rv, const uint8 *buff, int size, int wsize)
{
    rv.clear();

    rv.reserve(size+(size>>1));

    int last_seq_end=0; //точка конца последнего совпадения

    for(int i=0;i<size;)
    {
        //ищем максимальное совпадение

        int start=i-wsize; //начало поиска
        int seq_cnt=0; //длина совпадающей последовательности
        unsigned int offset=0; //смещение на последовательность
        if(start<0 || wsize<0)start=0;

        while(start<i)
        {
            //проверяем цепочки
            int k=0;
            while(buff[i+k]==buff[start+k])
            {
                k++;
                if(i+k==size)break;
            }

            if(k>=seq_cnt) //фиксируем результат
            {
                seq_cnt=k;
                offset=i-start-1;
            }
            start++;
        }

        //анализируем и пишем результат
        if(seq_cnt && !(seq_cnt==1 && (last_seq_end!=i || offset>buff[i])))
        {
            //если надо кодируем серию
            if(last_seq_end!=i)
            {
                unsigned int cnt=i-last_seq_end-1;
                cnt=_pref_sign2unsign(cnt);
                //формируем префиксный код
                _pref_p1qX_encoder(cnt,7,rv);
                rv.append((char*)&buff[last_seq_end],i-last_seq_end);
            }
            //кодируем смещение
            unsigned int cnt=-seq_cnt;
            cnt=_pref_sign2unsign(cnt);
            //формируем префиксный код счетчика
            _pref_p1qX_encoder(cnt,7,rv);
            //формируем префиксный код смещения
            _pref_p1qX_encoder(offset,7,rv);
            //корректируем точку конца совпадений и основной счетчик
            i=last_seq_end=i+seq_cnt;
        }
        else
        {
            i++;
        }
    }

    //если надо кодируем конечную серию
    if(last_seq_end!=size)
    {
        //кодируем знак
        unsigned int cnt=size-last_seq_end-1;
        cnt=_pref_sign2unsign(cnt);
        //формируем префиксный код
        _pref_p1qX_encoder(cnt,7,rv);
        rv.append((char*)&buff[last_seq_end],size-last_seq_end);
    }
}


void _arch_diction_decode(AData &rv, uint8 *buff, int size, int32 predict_size)
{
    rv.clear();

    if(predict_size<0)predict_size=size*2;
    rv.reserve(predict_size);

    for(int i=0;i<size;)
    {
        //читаем префиксный счетчик
        uint32 cnt;
        int k;
        if((k=_pref_p1q7_decoder(&cnt,&buff[i], size-i))<0){rv.clear();return;}
        i+=k;
        //делаем счетчик знаковым
        int32 scnt=_pref_unsign2sign(cnt);

        if(scnt<0) //смещение?
        { //todo: защита от ошибок!
            cnt=-scnt;
            //читаем смещение
            unsigned int offset;
            if((k=_pref_p1q7_decoder(&offset,&buff[i], size-i))<0){rv.clear();return;}
            i+=k;
            offset+=1;

            //восстанавливаем данные
            for(uint32 j=0;j<cnt;j++)rv.append(rv[rv.size()-offset]);
        }
        else //серия
        {
            cnt=scnt+1;
            if(i+cnt>(uint32)size){rv.clear();return;}
            //восстанавливаем данные
            for(uint32 j=0;j<cnt;j++)
            {
                rv.append(buff[i++]);
            }
        }
    }

}
