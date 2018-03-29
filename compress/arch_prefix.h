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

#ifndef PREFIX_CODE_LIBRARY_HEADER_DEFINITION
#define PREFIX_CODE_LIBRARY_HEADER_DEFINITION

#include "../types.h"
#include "../at_array.h"
#include "../abitop.h"
#include "../adata.h"

//кодирование знака
__inline uint32 _pref_sign2unsign(int32 val)
{
        if(val<0)return ((-val-1)<<1)|1;
        return (val<<1);
}

__inline int32 _pref_unsign2sign(uint32 val)
{
        if(val&1)return -((val>>1)+1);
        return (val>>1);
}

__inline uint64 _pref_sign2unsign64(int64 val)
{
        if(val<0)return ((-val-1)<<1)|1;
        return (val<<1);
}

__inline int64 _pref_unsign2sign64(uint64 val)
{
        if(val&1)return -((val>>1)+1);
        return (val>>1);
}

/*
        Код Голомба, является также общим случаем для Унарного кода и кодов Райса
        Рост кода линейный, поэтому он годится лишь для очень "хорошей" статистики.
*/
int _pref_golomb_size(unsigned int val, int p); //rise 2^k, unary - 1

/*
        Код Левенштейна, позже был повторно разработан Элиасом, который назвал его Гамма кодом
*/
int _pref_levenstain_size(unsigned int val);

/*
        Омега код Элиаса, одно из лучших приближений для log[2](val)
*/
int _pref_omega_size(unsigned int val);
void _pref_omega_encoder(unsigned int val, AAutoBitSpace &buff);
unsigned int _pref_omega_decoder(AAutoBitSpace &buff);
unsigned int _pref_omega_decoder(ABitSpace &buff);

/*
        Дельта код Элиаса, в особых статистических случаях может превзойти омегу,
но он имеет менее равномерный рост.
*/
int _pref_delta_size(unsigned int val);

/*
Примечания:
        Алгоритмы префиксного кодирования с фиксированным шагом PQ(S) разработал
Гришин М.Л.(altmer@arts-union.ru) в 2009г., независимо от возможных
аналогичных решений в данной области.
        PQS (шаг префикса, шаг диапазона значений остатка и параметр модификации
начального интервала) - регулируемый префиксный код. Основным назначением является
кодирование данных в потоках с фиксированным размером слова, например: байтовый
поток - код формата 1х7 (p=1, q=7); или в текстовых протоколах - 1х5 (система
счисления с основанием 64), 1x3 (Hex-строка), 1x2 (8-ричная строка) и др.; а также
полубайтных потоков - 4х4 или 1х3 и т.д. S - удлиннение начального интервала на
(2^s-1)*2^(q-s), а при s<0 простое добавление префикса длинной s бит для эффективного
кодирования нуля.
*/

//оптимизированная реализация наиболее востребованного варианта
int _pref_p1qX_size(uint32 val, int q);     //возвращает размер в словах

int _pref_p1qX_encoder(uint64 val, int q, AData &buff);
int _pref_p1qX_encoder(uint32 val, int q, AData &buff);
int _pref_p1qX_encoder(uint32 val, int q, ATArray<uint32> &buff);

int _pref_p1qX_decoder(uint32 *val, int q, ATArray<uint32> &buff);
int _pref_p1q7_decoder(uint32 *val, uint8 *buff, int size);
int _pref_p1q7_decoder(uint64 *val, uint8 *buff, int size);

//универсальная реализация
int _pref_pqs_size(unsigned int val, int p, int q, int s);
void _pref_pqs_encoder(unsigned int val, AAutoBitSpace &buff, int p, int q, int s);
unsigned int _pref_pqs_decoder(AAutoBitSpace &buff, int p, int q, int s);
unsigned int _pref_pqs_decoder(ABitSpace &buff, int p, int q, int s);

#endif
