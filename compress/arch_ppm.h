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

#ifndef PPM_CLASS_DEFINITION_HEADER
#define PPM_CLASS_DEFINITION_HEADER

#include "../atypes.h"

#define PPM_MODEL_SIMPLE        0

class PPM
{
private:
        int model;   //(0) - простая модель (по умолчанию)
                     //(-1) - модель с детерминированными данными (известной статистикой)
        uint32 buff[257];
public:
        //номер модели, предел потолка, максимальный алфавит(для выделения памяти)
        PPM();
        ~PPM();

        uint8 getModel();

        bool Reset(){return Reset(model);}
        bool Reset(int mod);     //сброс модели(обязательно перед использованием!!!)

        void Update(uint32 index);     //обновление модели на текущий символ
        int32 Scale();                //текущий потолок
        uint32 Hight(uint32 index);    //верхний диапазон
        uint32 Low(uint32 index);      //нижний диапазон
        int Index(uint32 count);    //индекс диапазона по значению
};

#endif
 
